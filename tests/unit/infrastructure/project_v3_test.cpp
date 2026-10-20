#include "../../support/classic_fixture.h"
#include "infrastructure/io/product_files.h"
#include "../../support/legacy_qxp/project_codec.h"
#include "qualpf/application/configured_fwhm.h"
#include "qualpf/application/initial_peak_search.h"
#include "qualpf/application/pfsm.h"
#include "qualpf/application/result_management.h"
#include "qualpf/application/workflow.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>
#include <numbers>
#include <source_location>
using namespace qualpf;
using namespace qualpf::application;
using namespace qualpf::infrastructure;
using namespace qualpf::infrastructure::legacy_fixture;
static void check(bool ok, std::source_location at = std::source_location::current()) {
    if (!ok)
        throw std::runtime_error("QXP v3 line " + std::to_string(at.line()));
}
template <class T> T take(Result<T> r, std::source_location at = std::source_location::current()) {
    if (!r)
        throw std::runtime_error("line " + std::to_string(at.line()) + " " + r.error().field + ": " +
                                 r.error().message);
    return std::move(r).value();
}
static double il(double x, double q) {
    return std::pow(1 + 4 * (std::pow(2., 2. / 3) - 1) * std::pow((x - q) / .2, 2), -1.5);
}
static ProductState state() {
    ProductState s;
    s.projectSettings.emplace();
    s.pattern.id = {17};
    s.pattern.source_id = "independent-analytic-mixture";
    s.pattern.radiation.lines = {{WavelengthAngstrom{1.5406}, 1}};
    s.pattern.processing = {false, false, false};
    for (int i = 0; i <= 700; ++i) {
        const double x = 28 + .01 * i;
        s.pattern.two_theta.emplace_back(x);
        s.pattern.observed.push_back(7 + 100 * il(x, 30.061) + 30 * il(x, 32.964));
    }
    s.pattern.original_observed = s.pattern.observed;
    s.pattern.original_kalpha2_subtracted = false;
    s.workflow.imported_pattern = std::make_shared<const Pattern>(s.pattern);
    s.projectSettings->profile(WorkflowId::preprocessing).background.method =
        preprocessing::BackgroundMethod::none;
    return s;
}
struct Reader : CandidateReader {
    int calls = 0;
    Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &,
                                          SearchProgress) override {
        ++calls;
        std::vector<ProductCard> cards;
        for (const auto &[id, q] :
             std::vector<std::pair<std::string, double>>{{"strong", 30}, {"weak", 33}, {"other", 31}}) {
            ProductCard c;
            c.id = id;
            c.d = {1.5406 / (2 * std::sin(q * std::numbers::pi / 360))};
            c.intensity = {1};
            c.referenceRir = 2;
            c.rir = "2";
            cards.push_back(c);
        }
        return Result<std::vector<ProductCard>>{std::move(cards)};
    }
};
static ProductState roundtrip(const ProductState &s) {
    auto encoded = take(encodeProject(std::make_shared<const ProductState>(s), {}, "test"));
    check(encoded["version"] == QJsonValue(3));
    const auto bytes = QJsonDocument(encoded).toJson();
    check(!bytes.contains("taskId") && !bytes.contains("checkpoints"));
    return take(decodeProject(QJsonDocument::fromJson(bytes).object(), SearchBackend::cpp, {}));
}
static void preprocessingRoundtrip() {
    ProductSearchSession session(state());
    auto source = session.snapshot();
    ExecutionContext context{{}, NumericalPolicy::legacy_mixed_precision, "qxp3"};
    auto result = take(evaluateInitialPeakSearch(
        source, source->projectSettings->profile(WorkflowId::preprocessing).initial_peaks, context));
    auto next = *source;
    take(applyInitialPeakSearch(next, result));
    take(session.commit(source, std::move(next)));
    source = session.snapshot();
    auto fixed = source->projectSettings->profile(WorkflowId::preprocessing).fwhm;
    fixed.use_fixed = true;
    fixed.default_fwhm = AngularWidthDegrees{.2};
    auto width = take(
        evaluateConfiguredFwhm(source, ConfiguredFwhmRequest{source->pattern, fixed, {}, true}, context));
    next = *source;
    take(applyConfiguredFwhm(next, width));
    take(session.commit(source, std::move(next)));
    check(bool(session.snapshot()->preprocessing.initial_peak_usage));
    check(!session.snapshot()->workflow.records.empty());
    auto restored = roundtrip(*session.snapshot());
    check(bool(restored.preprocessing.initial_peak_usage));
    check(restored.preprocessing.initial_peak_usage->detection.candidates.size() ==
          session.snapshot()->preprocessing.initial_peak_usage->detection.candidates.size());
    check(restored.preprocessing.fwhm_usage->width.value == .2 && restored.preprocessing.fwhm_usage->current);
    check(restored.workflow.records.size() == session.snapshot()->workflow.records.size());
    check(reusableWorkflowStep(restored, WorkflowId::csm, WorkflowStep::background));
    ProductSearchSession opened(restored);
    for (const auto &r : restored.workflow.records)
        check(!opened.canUndo(r.id));
    auto encoded = take(encodeProject(opened.snapshot(), {}, "test"));
    encoded["_qxp3_unknown_state"] = QJsonArray{false, 42};
    auto quantExtra = encoded["quantification"].toObject();
    quantExtra["future"] = QJsonArray{false, QJsonValue::Null, qint64(9007199254740993LL)};
    encoded["quantification"] = quantExtra;
    auto block = encoded["state"].toObject();
    block["unknown"] = QJsonObject{{"wide", qint64(9007199254740993LL)}, {"null", QJsonValue::Null}};
    encoded["state"] = block;
    auto unknown = take(decodeProject(encoded, SearchBackend::cpp, {}));
    auto again = take(encodeProject(std::make_shared<const ProductState>(unknown), {}, "test"));
    check(again["state"].toObject()["unknown"] == block["unknown"] &&
          again["_qxp3_unknown_state"] == encoded["_qxp3_unknown_state"] &&
          again["quantification"].toObject()["future"] == quantExtra["future"]);
    auto bad = encoded;
    block = bad["state"].toObject();
    auto pattern = block["pattern"].toObject();
    pattern["observed"] = QJsonArray{1};
    block["pattern"] = pattern;
    bad["state"] = block;
    check(!decodeProject(bad, SearchBackend::cpp, {}));
    bad = encoded;
    block = bad["state"].toObject();
    auto workflow = block["workflow"].toObject();
    auto records = workflow["records"].toArray();
    auto record = records[0].toObject();
    record["dependencies"] = QJsonArray{"999"};
    records[0] = record;
    workflow["records"] = records;
    block["workflow"] = workflow;
    bad["state"] = block;
    check(!decodeProject(bad, SearchBackend::cpp, {}));
    QTemporaryDir directory;
    check(directory.isValid());
    const auto path = (directory.path() + "/saved.qxp").toStdString();
    ProjectFileWriter writer;
    take(saveProjectInput(session.snapshot(), writer, path, {}, "test"));
    auto prior = take(QtTextFiles{}.read(path));
    struct FailingDefaults : DefaultSettingsStore {
        mutable int reads = 0;
        bool unavailable = false;
        Result<ProjectSettings> load() const override {
            ++reads;
            if (!unavailable)
                return Result<ProjectSettings>{ProjectSettings{}};
            return Result<ProjectSettings>{Error{ErrorCode::missing_data, "defaults", "unavailable"}};
        }
        Status save(const ProjectSettings &) override { return Status{std::monostate{}}; }
    } defaults;
    ProductSearchSession independent(SearchBackend::cpp, defaultAlgorithms(), &defaults);
    defaults.reads = 0;
    defaults.unavailable = true;
    take(openProjectInput(independent, ProjectFileReader{}, path, {}));
    check(defaults.reads == 0 && independent.snapshot()->projectSettings.has_value());

    auto invalid = *session.snapshot();
    invalid.pattern.observed = {1};
    check(!saveProjectInput(std::make_shared<const ProductState>(invalid), writer, path, {}, "fail"));
    check(take(QtTextFiles{}.read(path)) == prior);
    take(openProjectInput(session, ProjectFileReader{}, path, {}));
    for (const auto &r : session.snapshot()->workflow.records)
        check(!session.canUndo(r.id));
}
static void csmRoundtrip() {
    testing::ClassicFixture fixture;
    ProductState s;
    s.pattern = fixture.pattern;
    s.pattern.radiation.lines = {{WavelengthAngstrom{1.54}, 1}};
    auto peaks = fixture.peaks;
    peaks.effective_wavelength = WavelengthAngstrom{1.54};
    s.peaks = take(productMeasuredPeaks(peaks));
    struct CsmReader : CandidateReader {
        ProductCard card;
        Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &,
                                              SearchProgress) override {
            return Result<std::vector<ProductCard>>{{card}};
        }
    } reader;
    reader.card.id = "A";
    reader.card.rir = "2";
    for (const auto &reflection : fixture.card.reflections) {
        reader.card.tth.push_back(reflection.two_theta->value);
        reader.card.intensity.push_back(reflection.intensity);
    }
    SearchInput input;
    input.source = std::make_shared<const ProductState>(s);
    input.parameters = fixture.parameters;
    input.databaseVersion = 9;
    input.databaseIdentity = "classic-library";
    auto result = take(SearchEngine{}.run(input, reader, fixture.context)).next;
    check(result.candidates.size() == 1 && result.candidates[0].fom == 1 && result.candidates[0].scale == 10);
    auto restored = roundtrip(result);
    check(restored.candidates[0].fom == 1 && restored.candidates[0].scale == 10 &&
          restored.candidates[0].tth == reader.card.tth);
    check(restored.candidates[0].algorithmSource &&
          restored.candidates[0].algorithmSource->score.algorithms == restored.algorithms);
    auto accept = take(
        phaseAcceptanceInput(std::make_shared<const ProductState>(restored), "A", fixture.parameters, true));
    check(bool(accept));
    accept->databaseIdentity = input.databaseIdentity;
    accept->databaseVersion = 20;
    auto selected = take(SearchEngine{}.run(*accept, reader, fixture.context)).next;
    restored = roundtrip(selected);
    check(restored.phases.size() == 1 && restored.peaks.intensity == selected.peaks.intensity &&
          restored.provenance.residualHistory.size() == selected.provenance.residualHistory.size());
}
static void pfsm(bool recall, bool cache) {
    auto s = state();
    s.algorithms = take(resolveAlgorithms(AlgorithmSpec{{}, SearchMethod::pfsm}));
    s.workflow.active = WorkflowId::pfsm;
    // Explicit, independently generated signal and background; no product
    // goldens.
    auto request = std::make_shared<PfsmRequest>();
    request->threads = 1;
    request->template_cache = cache;
    request->lower_shift = -.12;
    request->upper_shift = .12;
    request->candidate_recall.mode = recall ? PfsmCandidateRecallMode::enabled : PfsmCandidateRecallMode::off;
    request->observations.emplace();
    auto &o = *request->observations;
    o.signal_source = "analytic";
    o.intensity_units = "arbitrary";
    o.noise_source = "equal weights";
    o.unclipped_signal = true;
    o.equal_weights = true;
    o.background = std::vector<double>(701, 7);
    auto source = std::make_shared<const ProductState>(s);
    SearchInput input;
    input.source = source;
    input.method = SearchMethod::pfsm;
    input.pfsm = request;
    input.databaseVersion = 19;
    input.databaseIdentity = "fixture:immutable-v1";
    ExecutionContext context{{}, NumericalPolicy::explicit_float64, "qxp3"};
    Reader reader;
    auto searched = take(SearchEngine{}.run(input, reader, context));
    check(searched.next.candidates.front().id == "strong");
    auto accepted =
        take(phaseAcceptanceInput(std::make_shared<const ProductState>(searched.next), "strong", SearchParameters{AngularWidthDegrees{0}}, true));
    check(bool(accepted));
    accepted->databaseVersion = 19;
    accepted->databaseIdentity = input.databaseIdentity;
    auto selected = take(SearchEngine{}.run(*accepted, reader, context)).next;
    selected = take(preparePfsmRirRevision(std::make_shared<const ProductState>(selected), "strong",
                                           PfsmRirRevision{3, "independent supplied RIR"}));
    QTemporaryDir savedDirectory;
    check(savedDirectory.isValid());
    const auto savedPath = (savedDirectory.path() + "/pfsm.qxp").toStdString();
    take(saveProjectInput(std::make_shared<const ProductState>(selected), ProjectFileWriter{}, savedPath, {},
                          "pfsm"));
    ProductSearchSession fresh;
    take(openProjectInput(fresh, ProjectFileReader{}, savedPath, {}));
    check(fresh.snapshot()->pfsm && fresh.snapshot()->algorithms->spec.method() == SearchMethod::pfsm);
    check(fresh.snapshot()->pfsm->baseline.parameters.shifts == selected.pfsm->baseline.parameters.shifts);
    auto restored = roundtrip(selected);
    check(restored.algorithms == restored.pfsm->problem->algorithms);
    check(restored.pfsm->baseline.calculated == selected.pfsm->baseline.calculated &&
          restored.pfsm->baseline.parameters.shifts == selected.pfsm->baseline.parameters.shifts);
    check(restored.pfsm->normalization_maximum == selected.pfsm->normalization_maximum &&
          restored.pfsm->rir_revisions == selected.pfsm->rir_revisions);
    check(prepareQuantification(std::make_shared<const ProductState>(restored)).available());
    check(prepareQuantification(std::make_shared<const ProductState>(restored)).percentages() ==
          prepareQuantification(std::make_shared<const ProductState>(selected)).percentages());
    auto resume =
        take(phaseAcceptanceInput(std::make_shared<const ProductState>(restored), "weak", SearchParameters{AngularWidthDegrees{0}}, true));
    check(bool(resume));
    resume->databaseVersion = 200;
    resume->databaseIdentity = input.databaseIdentity;
    auto continued = take(SearchEngine{}.run(*resume, reader, context));
    check(continued.next.phases.size() == 2 && reader.calls == 1);
    auto direct =
        take(phaseAcceptanceInput(std::make_shared<const ProductState>(selected), "weak", SearchParameters{AngularWidthDegrees{0}}, true));
    direct->databaseVersion = 19;
    direct->databaseIdentity = input.databaseIdentity;
    auto reference = take(SearchEngine{}.run(*direct, reader, context));
    check(continued.next.pfsm->baseline.parameters.shifts ==
              reference.next.pfsm->baseline.parameters.shifts &&
          continued.next.pfsm->baseline.q == reference.next.pfsm->baseline.q);
    resume->databaseIdentity = "another-library";
    auto denied = SearchEngine{}.run(*resume, reader, context);
    check(!denied && denied.error().reason == "project_database_mismatch" && reader.calls == 1);
    auto encoded = take(encodeProject(std::make_shared<const ProductState>(restored), {}, "test"));
    auto stateJson = encoded["state"].toObject();
    auto analysis = stateJson["pfsm"].toObject();
    auto baseline = analysis["baseline"].toObject();
    auto values = baseline["calculated"].toArray();
    values[100] = values[100].toDouble() + 1;
    baseline["calculated"] = values;
    analysis["baseline"] = baseline;
    stateJson["pfsm"] = analysis;
    encoded["state"] = stateJson;
    check(!decodeProject(encoded, SearchBackend::cpp, {}));
    check(!encodeLegacyProject(std::make_shared<const ProductState>(selected), {}, "unsupported"));
}
int main(int argc, char **argv) try {
    QCoreApplication app(argc, argv);
    preprocessingRoundtrip();
    csmRoundtrip();
    pfsm(false, false);
    pfsm(true, true);
    std::cout << "PASS: QXP v3 workflow evidence, unknown fields, atomic save, "
                 "PFSM continuation and database admission\n";
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}
