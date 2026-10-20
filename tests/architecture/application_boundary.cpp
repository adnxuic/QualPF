#include "qualpf/application/database_build.h"
#include "qualpf/application/algorithms.h"
#include "qualpf/application/geometric_fwhm.h"
#include "qualpf/application/domain_inputs.h"
#include "qualpf/application/environment.h"
#include "qualpf/application/kalpha2.h"
#include "qualpf/application/platform_services.h"
#include "qualpf/application/plot_coordinates.h"
#include "qualpf/application/preferences.h"
#include "qualpf/application/product_files.h"
#include "qualpf/application/reference_database.h"
#include "qualpf/application/update_service.h"
#include "qualpf/application/database/cif_batch.h"
#include "qualpf/application/export/atomic_export_writer.h"
#include "qualpf/application/import/powder_cif_import_service.h"
#include "qualpf/application/presentation/plot_snapshot.h"
#include "qualpf/application/presentation/quantification.h"
#include "qualpf/application/startup/application_initialization_service.h"
#include "qualpf/application/powder_cif_source.h"
#include "qualpf/application/preprocessing_evaluation.h"
#include "qualpf/application/search_coordinator.h"
#include "qualpf/application/search_preparation.h"
#include "qualpf/application/preprocessing_workflows.h"
#include "qualpf/application/result_management.h"
#include <cmath>
#include "qualpf/application/workspace_ports.h"
#include <iostream>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
// The single-module assembly example in search-algorithms.md is compiled here.
class ExampleMatcher final : public search::IClassicMatcher {
  public:
    AlgorithmIdentity identity() const override { return {"example.classic-matcher", 1}; }
    AlgorithmIdentity produces() const override { return search::classic_match_contract(); }
    Result<search::ClassicMatch> match(const search::ClassicMatchInput &input, const ExecutionContext &context) const override {
        return search::ClassicMatcher{}.match(input, context);
    }
};
static std::unique_ptr<Workspace> singleModuleWorkspace() {
    auto matcher = std::make_shared<const ExampleMatcher>();
    AlgorithmSelections selections;
    selections.matcher = matcher->identity();
    AlgorithmModules overrides;
    overrides.matcher = matcher;
    auto resolved = resolveAlgorithms(AlgorithmSpec{selections}, std::move(overrides));
    if (!resolved)
        throw std::runtime_error(resolved.error().message);
    return std::make_unique<Workspace>(SearchBackend::cpp, resolved.value());
}
struct Reader : PatternSourceReader {
    Result<Pattern> read(const std::string &) const override {
        return Result<Pattern>{Error{ErrorCode::missing_data, "reader", "controlled failure"}};
    }
};
struct CifReader : CifSourceReader {
    int reads = 0;
    std::variant<std::string, CifImportFailure> read(const std::string &path, std::size_t) override {
        ++reads;
        return CifImportFailure{
            "read", "controlled_read_failure", path, {}, "file", "Controlled reader failure", {}, {}, {}, {}};
    }
};
struct CifWriter : CifRecordWriter {
    int creates = 0, writes = 0, closes = 0;
    std::optional<database::WriteError> create(const std::string &) override {
        ++creates;
        return {};
    }
    std::optional<database::WriteError> write(const database::CifRecord &) override {
        ++writes;
        return {};
    }
    void close() override { ++closes; }
};
static void cifPorts() {
    CifReader reader;
    CifWriter writer;
    const std::vector<CifBatchRequest> sources{{"owned-source", 7, {}}};
    auto report = runCifBatch(writer, reader, "owned-database", sources, CifDatabaseBackend::cpp);
    if (!report.completed || report.failures.size() != 1 || report.committed || writer.writes ||
        reader.reads != 1 || writer.closes != 1)
        throw std::runtime_error("CIF failed read published a card or violated batch policy");
    std::stop_source stop;
    CifBatchOptions options;
    options.stop = stop.get_token();
    report = runCifBatch(writer, reader, "owned-database", sources, CifDatabaseBackend::cpp, options,
                         [&](const auto &, const auto &) { stop.request_stop(); });
    if (!report.cancelled || report.completed || reader.reads != 1 || writer.writes || writer.closes != 2)
        throw std::runtime_error("CIF cancellation boundary read or committed data");
    report = runCifBatch(
        writer, reader, "owned-database", sources, CifDatabaseBackend::cpp, {},
        [](const auto &, const auto &) { throw std::runtime_error("controlled callback failure"); });
    if (!report.fatal || report.fatal->code != "cif_callback_or_operation_failure" || report.completed ||
        writer.closes != 3)
        throw std::runtime_error("CIF callback exception escaped cleanup");
    stop.request_stop();
    options.stop = stop.get_token();
    const int before = writer.creates;
    report = runCifBatch(writer, reader, "owned-database", sources, CifDatabaseBackend::cpp, options);
    if (!report.cancelled || writer.creates != before)
        throw std::runtime_error("pre-cancel CIF created a database");
}
// Full product workflow, linked against exactly the product Application target.
struct ExampleCandidates : CandidateReader {
    int reads = 0;
    Result<std::vector<ProductCard>> read(const CandidateQuery &query, const ExecutionContext &, SearchProgress) override {
        ++reads;
        if (!query.decodeReflections) throw std::runtime_error("scored read requirements");
        ProductCard card; card.id = "analytic"; card.quality = "A"; card.rir = "1";
        card.tth = {15,23,32}; card.intensity = {10,20,15};
        return Result<std::vector<ProductCard>>{std::vector<ProductCard>{card}};
    }
};
static void completeClassicalWorkflow(Workspace &workspace) {
    Pattern pattern;
    pattern.id = DatasetId{1};
    pattern.radiation = {RadiationKind::x_ray, {{WavelengthAngstrom{1.54}, 1}}};
    pattern.processing.background_subtracted = false;
    pattern.processing.intensity_scaled = false;
    pattern.intensity_scale = IntensityScale::counts;
    for (int i = 0; i <= 1500; ++i) {
        const double x = 10 + .02 * i;
        double y = 5;
        for (auto [center,height] : {std::pair{15.,100.}, {23.,200.}, {32.,150.}})
            y += height * std::exp(-.5 * std::pow((x-center)/.12,2));
        pattern.two_theta.push_back(TwoThetaDegrees{x}); pattern.observed.push_back(y);
    }
    pattern.calculation_range = AngularRange{pattern.two_theta.front(), pattern.two_theta.back()};
    auto require = [](const auto &value) { if (!value) throw std::runtime_error(value.error().field + ": " + value.error().message); };
    require(workspace.session().openPattern(workspace.session().snapshot(), std::move(pattern)));
    auto source = workspace.session().snapshot();
    // Standalone stages remain temporary; they neither publish nor search.
    CommonPreprocessingRequest common;
    common.background = source->algorithms->spec.defaults().background;
    common.peaks = source->algorithms->spec.defaults().peaks; common.peaks->search_range = -1;
    common.wavelength = WavelengthAngstrom{1.54}; common.purpose = AnalysisPurpose::peak_measurement;
    const ExecutionContext context{{}, NumericalPolicy::legacy_mixed_precision, "qtfree-workflow"};
    auto preprocessed = evaluateCommonPreprocessing(source, common, context); require(preprocessed);
    auto parameters = preprocessing::prepare_classic_peak_parameters(*common.peaks, preprocessed.value().fwhm->estimate,
        common.wavelength, context); require(parameters);
    auto formal = evaluateFormalPeakExtraction(source, {preprocessed.value().spectrum, parameters.value(),
        common.wavelength, AngularWidthDegrees{.15}}, context); require(formal);
    if (workspace.session().snapshot() != source || formal.value().peaks.peaks.size() < 3)
        throw std::runtime_error("standalone stages published or lost analytic peaks");
    auto preparation = prepareSearch(source, SearchMethod::classic_sm); require(preparation);
    if (preparation.value().state != SearchPreparation::State::needs_peak_preparation)
        throw std::runtime_error("automatic preparation decision");
    ProductPreprocessingEdit edit(workspace.session());
    require(edit.preview(*preparation.value().peaks)); require(edit.commit());
    source = workspace.session().snapshot(); // Peak preparation is independently committed.
    auto ready = prepareSearch(source, SearchMethod::classic_sm); require(ready);
    if (ready.value().state != SearchPreparation::State::ready || ready.value().peaks)
        throw std::runtime_error("existing peak table was prepared twice");
    SearchSettingsSnapshot settings;
    settings.parameters.delta_two_theta = AngularWidthDegrees{.25};
    settings.parameters.weight_position = settings.parameters.weight_intensity = settings.parameters.weight_phases = .5;
    settings.parameters.minimum_score = .35; settings.parameters.maximum_entries = 20;
    settings.parameters.strongest_count = 3; settings.parameters.include_deleted_cards = true;
    settings.parameters.filter_strongest = false;
    auto input = matchingInput(source, settings); require(input);
    auto task = workspace.searches().begin(); require(task);
    input.value().taskId = task.value().id;
    ExampleCandidates reader;
    auto result = SearchEngine{task.value().algorithms}.run(input.value(), reader, task.value().context);
    auto published = workspace.searches().complete(task.value().id, std::move(result)); require(published);
    source = workspace.session().snapshot();
    if (source->candidates.size() != 1 || !source->candidates[0].fomCalculated)
        throw std::runtime_error("complete Classical candidate publication");
    auto acceptance = phaseAcceptanceInput(source, "analytic", settings.parameters, true); require(acceptance);
    if (!acceptance.value()) throw std::runtime_error("missing identity intent");
    auto acceptedTask = workspace.searches().begin(); require(acceptedTask);
    acceptance.value()->taskId = acceptedTask.value().id;
    result = SearchEngine{acceptedTask.value().algorithms}.run(*acceptance.value(), reader, acceptedTask.value().context);
    published = workspace.searches().complete(acceptedTask.value().id, std::move(result)); require(published);
    source = workspace.session().snapshot();
    if (reader.reads != 1 || source->phases.size() != 1 || source->provenance.residualHistory.size() != 1)
        throw std::runtime_error("multiphase continuation requeried or lost its atomic residual");
    const auto &phase = source->phases.front();
    const std::vector<QuantificationInput> phases{{phase.fomCalculated, phase.intensity, phase.scale, 1}};
    const auto percentages = quantifyProduct(*source, phases);
    if (percentages.size() != 1 || std::abs(percentages[0]-100) > 1e-12)
        throw std::runtime_error("fixed quantitative workflow");
}
static void geometricWidth() {
    qualpf::application::GeometricFwhmRequest request;
    request.input.processing.background_subtracted = false;
    request.input.dataset = {91};
    for (int i = 0; i <= 20; ++i) {
        request.input.two_theta.emplace_back(20 + .01 * i);
        request.input.intensity.push_back(28 - std::abs(i - 10));
        request.input.background.push_back(20);
    }
    qualpf::application::ProductState state; state.pattern.id = request.input.dataset;
    qualpf::application::ProductSearchSession session(state);
    auto source = session.snapshot();
    auto result = qualpf::application::evaluateGeometricFwhm(source, request,
        qualpf::ExecutionContext{{}, qualpf::NumericalPolicy::explicit_float64, {}});
    if (!result) throw std::runtime_error(result.error().message);
    if (result.value().source != source || session.snapshot() != source ||
        !result.value().measurement.default_fwhm ||
        std::abs(result.value().measurement.default_fwhm->value - .08) > 1e-12)
        throw std::runtime_error("pure geometric FWHM evaluation");
}
int main() try {
    geometricWidth();
    auto selected = singleModuleWorkspace();
    const auto algorithms = selected->session().snapshot()->algorithms;
    if (algorithms->modules.matcher->identity().id != "example.classic-matcher" ||
        algorithms->modules.fwhm->identity() != preprocessing::ClassicFwhmEstimator{}.identity())
        throw std::runtime_error("explicit single-module assembly");
    auto pfsm = resolveAlgorithms(AlgorithmSpec{{}, SearchMethod::pfsm});
    if (!pfsm || !pfsm.value()->modules.profile_expander || !pfsm.value()->modules.profile_fitter)
        throw std::runtime_error("PFSM seven-module assembly failed");
    completeClassicalWorkflow(*selected);
    cifPorts();
    const std::vector<double> intensity{25, 100};
    const std::vector<QuantificationInput> phases{{true, intensity, 1.5, 2.0}};
    const auto percentages = quantify(phases);
    if (percentages.size() != 1 || percentages[0] != 100)
        throw std::runtime_error("pure quantification interface");
    Workspace workspace;
    auto source = workspace.session().snapshot();
    if (openPatternInput(workspace.session(), Reader{}, "fixture") ||
        workspace.session().snapshot() != source)
        throw std::runtime_error("failed IO published state");
    SearchSettingsSnapshot settings;
    settings.parameters.maximum_entries = 0;
    auto input = matchingInput(source, settings);
    if (!input || input.value().parameters.maximum_entries != 0 || input.value().source != source)
        throw std::runtime_error("owned input contract");
    auto standalone = ApplicationInitializationService{}.createWorkspace();
    auto task = standalone->searches().begin();
    if (!task || task.value().source != standalone->session().snapshot())
        throw std::runtime_error("composition contract");
    std::cout << "PASS: complete Application consumed by a Qt-free C++20 client\n";
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
}
