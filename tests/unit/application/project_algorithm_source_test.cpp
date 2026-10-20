#include "classic_fixture.h"
#include "gui/adapters/quantification.h"
#include "infrastructure/io/product_files.h"
#include "../../support/legacy_qxp/project_codec.h"
#include "infrastructure/serialization/quantification.h"
#include "qualpf/application/preprocessing_evaluation.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
using namespace qualpf::infrastructure::legacy_fixture;
static void check(bool v, std::source_location at = std::source_location::current()) {
    if (!v) throw std::runtime_error("Project algorithm source at line " + std::to_string(at.line()));
}
struct Matcher : search::IClassicMatcher {
    mutable int calls = 0;
    AlgorithmIdentity identity() const override { return {"test.project.matcher", 1}; }
    AlgorithmIdentity produces() const override { return search::classic_match_contract(); }
    Result<search::ClassicMatch> match(const search::ClassicMatchInput &input, const ExecutionContext &ctx) const override {
        ++calls; return search::ClassicMatcher{}.match(input, ctx);
    }
};
struct Reader : CandidateReader {
    Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &, SearchProgress) override {
        throw std::runtime_error("Unverified project must reject before database work");
    }
};
int main(int argc, char **argv) try {
    QCoreApplication app(argc, argv); testing::ClassicFixture f;
    ProductState s; s.pattern = f.pattern; s.pattern.radiation.lines = {{WavelengthAngstrom{1.54},1}};
    s.pattern.original_observed = s.pattern.observed; s.pattern.original_kalpha2_subtracted = false;
    s.pattern.processing.background_subtracted = false; s.pattern.processing.intensity_scaled = false;
    auto p = f.peaks; p.effective_wavelength = WavelengthAngstrom{1.54}; s.peaks = productMeasuredPeaks(p).value();
    ProductCard a; a.id = "A"; a.fomCalculated = true; a.fom = 1; a.scale = 2; a.rir = " 2 "; a.d = {3,2,1}; a.intensity = {10,20,10};
    auto b = a; b.id = "B"; b.scale = 3; b.rir = "1"; s.phases = {a,b}; s.candidates = {a};
    const auto source = std::make_shared<const ProductState>(s);
    auto prepared = prepareProjectSerialization(source, infrastructure::quantificationInputs(source->phases));
    check(prepared && prepared.value().percentages() == std::vector<double>({25,75}) &&
          prepared.value().algorithms() == source->algorithms && prepared.value().quantitativeValid() && !prepared.value().legacyUnverified());
    // Exact binary64 rounding proof: rounded(100*x)/x is
    // 49959931866296704 / 499599318662967, which rounds to 100 + 1 ULP.
    // The approved multiplication-before-division order must remain saveable.
    auto rounded = s; auto single = a;
    single.intensity = {0x1.54c985f06f694p-10}; single.d = {2}; single.scale = 1; single.rir = "1";
    rounded.phases = {single}; rounded.candidates.clear();
    const auto roundedSource = std::make_shared<const ProductState>(rounded);
    auto rounding = prepareProjectSerialization(roundedSource, infrastructure::quantificationInputs(roundedSource->phases));
    check(rounding && rounding.value().percentages() == std::vector<double>{0x1.9000000000001p+6});
    auto roundedEncoding = infrastructure::legacy_fixture::encodeLegacyProject(rounding.value(), {}, "ordered-binary64");
    check(roundedEncoding && roundedEncoding.value()["phases"].toArray()[0].toObject()["weight_percent"].toDouble() == 0x1.9000000000001p+6);
    auto encoded = infrastructure::legacy_fixture::encodeLegacyProject(prepared.value(), {}, "frozen-clock"); check(bool(encoded));
    const auto root = encoded.value(); const auto phases = root["phases"].toArray();
    check(root["version"].toInt() == 2 && phases[0].toObject()["weight_percent"].toDouble() == 25 && phases[1].toObject()["weight_percent"].toDouble() == 75);
    check(root.keys() == QStringList({"automatic_delta","created","diffraction_data","peaks","phases","preprocessing","restraints","results","search_options","version"}));
    check(phases[0].toObject().keys() == QStringList({"d","fom","fom_calculated","fom_intensity","fom_peakpos","formula","id","intensity","mineral","name","quality","rir","scale","spg","weight_percent"}));
    QTemporaryDir directory; check(directory.isValid()); const auto path = directory.path() + "/saved.qxp";
    struct LegacyWriter : qualpf::infrastructure::legacy_fixture::ProjectTargetWriter {
        Result<std::string> prepare(ProductHandle s,const MetadataObject &d,const std::string &c) const override {
            auto encoded=infrastructure::legacy_fixture::encodeLegacyProject(std::move(s),d,QString::fromStdString(c));
            if(!encoded)return Result<std::string>{encoded.error()};
            return Result<std::string>{QJsonDocument(encoded.value()).toJson().toStdString()};
        }
        Status commit(const std::string &p,const std::string &s) const override { return infrastructure::legacy_fixture::ProjectFileWriter{}.commit(p,s); }
    } writer;
    check(bool(saveProjectInput(source, writer, path.toStdString(), {}, "frozen-clock")));
    QFile file(path); check(file.open(QIODevice::ReadOnly)); const auto original = file.readAll(); file.close();
    check(QJsonDocument::fromJson(original).object() == root);
    auto loaded = infrastructure::legacy_fixture::readProject(path, SearchBackend::cpp, {}); check(loaded && loaded.value().provenance.legacyUnverified && loaded.value().phases[0].fomCalculated);
    ProductSearchSession defaultSession;
    check(bool(openProjectInput(defaultSession, infrastructure::legacy_fixture::ProjectFileReader{}, path.toStdString(), {})));
    check(defaultSession.snapshot()->provenance.legacyUnverified && defaultSession.snapshot()->phases[0].fomCalculated);
    check(gui::quantitativePercentages(defaultSession.snapshot()) == std::vector<double>({25,75}));
    Reader reader; SearchInput input; input.source = defaultSession.snapshot(); input.parameters = f.parameters;
    auto denied = SearchEngine{}.run(input, reader, f.context);
    check(!denied && denied.error().reason == "legacy_algorithm_provenance_unverified");
    check(!SearchEngine{}.rescore(*input.source, f.parameters, f.context));
    check(!SearchEngine{}.residual(*input.source, a, f.parameters, f.context));
    auto matcher = std::make_shared<Matcher>(); AlgorithmSelections selected; selected.matcher = matcher->identity(); AlgorithmModules modules; modules.matcher = matcher;
    const auto algorithms = resolveAlgorithms(AlgorithmSpec{selected}, modules).value(); s.algorithms = algorithms;
    const auto custom = std::make_shared<const ProductState>(s);
    check(gui::quantitativePercentages(custom) == std::vector<double>({25,75}) && matcher->calls == 0);
    auto unsupported = saveProjectInput(custom, writer, path.toStdString(), {}, "must-not-write");
    check(!unsupported && unsupported.error().reason == "qxp_algorithm_provenance_unsupported" && matcher->calls == 0);
    check(!writer.prepare(custom, {}, "must-not-write") && !infrastructure::legacy_fixture::encodeLegacyProject(custom, {}, "must-not-write"));
    check(file.open(QIODevice::ReadOnly)); check(file.readAll() == original); file.close();
    ProductSearchSession customSession(SearchBackend::cpp, algorithms);
    check(bool(openProjectInput(customSession, infrastructure::legacy_fixture::ProjectFileReader{}, path.toStdString(), {})));
    input.source = customSession.snapshot();
    check(input.source->algorithms == algorithms && input.source->provenance.legacyUnverified &&
          !input.source->phases[0].fomCalculated && !input.source->candidates[0].fomCalculated && gui::quantitativePercentages(input.source).empty() && matcher->calls == 0);
    check(!SearchEngine{}.run(input, reader, f.context));
    PreprocessingRequest restart; restart.restart_from_original = true;
    PreprocessingEvaluation evaluation(input.source); check(bool(evaluation.preview(restart)));
    check(!evaluation.preview()->provenance.legacyUnverified && evaluation.preview()->phases.empty() &&
          evaluation.preview()->candidates.empty() && !evaluation.preview()->peaks.valid &&
          evaluation.preview()->pattern.observed == *input.source->pattern.original_observed);
    // Restoring the default selection cannot launder a previously non-default source.
    s.algorithms = defaultAlgorithms(); AlgorithmRunSource previous; previous.algorithms = algorithms;
    s.provenance.peakSource = std::move(previous);
    check(!validateLegacyProjectSaveSource(std::make_shared<const ProductState>(s)));
    auto v1 = root; v1["version"] = 1;
    check(bool(infrastructure::legacy_fixture::decodeProject(v1, SearchBackend::cpp, {})));
    std::cout << "PASS: fixed RIR preparation with selected matching provenance, unchanged default QXP, atomic unsupported save and unverified legacy restart semantics\n";
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
