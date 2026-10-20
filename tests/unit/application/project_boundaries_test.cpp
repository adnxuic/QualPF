#include "gui/model/cardtype.h"
#include "infrastructure/serialization/metadata_values.h"
#include "../../support/legacy_qxp/project_codec.h"
#include "infrastructure/serialization/quantification.h"
#include "qualpf/domain/radiation.h"
#include "qualpf/application/configured_fwhm.h"
#include "qualpf/application/initial_peak_search.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QTemporaryDir>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
using namespace qualpf::infrastructure::legacy_fixture;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
std::vector<double> originalQuant(const std::vector<ProductCard> &phases) {
    QVector<double> rap(static_cast<int>(phases.size()), 0.0);
    bool valid = true;
    for (int i = 0; i < rap.size(); ++i) {
        if (!phases[i].fomCalculated) {
            valid = false;
            break;
        }
        bool ok;
        const double rir = QString::fromStdString(phases[i].rir).trimmed().toDouble(&ok);
        if (!ok || rir <= 0.0) {
            valid = false;
            break;
        }
        double maxI = 0.0;
        for (double value : phases[i].intensity)
            maxI = qMax(maxI, value * phases[i].scale);
        rap[i] = maxI / rir;
    }
    double sum = 0.0;
    for (double r : rap)
        sum += r;
    QVector<double> quant(rap.size(), 0.0);
    if (valid && sum > 0.0)
        for (int i = 0; i < rap.size(); ++i)
            quant[i] = 100.0 * rap[i] / sum;
    return valid && sum > 0.0 ? std::vector<double>(quant.begin(), quant.end()) : std::vector<double>{};
}
void checkQuant() {
    ProductCard first;
    first.fomCalculated = true;
    first.scale = 0.713576;
    first.intensity = {2, 100.125, 6};
    first.rir = " 2.345 ";
    ProductCard second = first;
    second.scale = 3.417;
    second.rir = "1.75";
    for (const auto &rir : {"1.75", "0", "-1", "invalid", "nan", "inf", "  2.5  "})
        for (double scale : {0.0, -2.0, 0.7912, std::numeric_limits<double>::infinity()})
            for (bool calculated : {false, true}) {
                second.rir = rir;
                second.scale = scale;
                second.fomCalculated = calculated;
                const std::vector<ProductCard> cards{first, second};
                const auto expected = originalQuant(cards),
                           actual = infrastructure::quantitativePercentages(cards);
                require(expected.size() == actual.size(), "Quantification validity changed");
                for (std::size_t i = 0; i < actual.size(); ++i)
                    require(std::bit_cast<std::uint64_t>(expected[i]) ==
                                std::bit_cast<std::uint64_t>(actual[i]),
                            "Quantification value changed");
            }
    require(infrastructure::quantitativePercentages({}).empty(), "Empty quantification changed");
}
struct FailedProject : qualpf::infrastructure::legacy_fixture::ProjectSourceReader {
    Result<ProductState> read(const std::string &, SearchBackend, const MetadataObject &) const override {
        return Result<ProductState>{Error{ErrorCode::missing_data, "project.file", "controlled failure"}};
    }
};
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        checkQuant();
        ProductState state;
        state.projectEnvelope["future"] =
            MetadataObject{{"wide", std::int64_t{9007199254740993LL}},
                           {"null", nullptr},
                           {"array", MetadataArray{std::int64_t{3}, 3.5, "value"}}};
        state.projectMetadata["unknown"] =
            MetadataObject{{"integer", std::int64_t{-9007199254740993LL}}, {"null", nullptr}};
        state.pattern.id = {1};
        state.pattern.source_id = "owned-pattern";
        state.pattern.two_theta = {TwoThetaDegrees{20}, TwoThetaDegrees{40}};
        state.pattern.observed = {12, 24};
        state.pattern.radiation.lines = {{radiation::default_wavelength, 1}};
        state.pattern.calculation_range = AngularRange{TwoThetaDegrees{20}, TwoThetaDegrees{40}};
        ProductCard card;
        card.id = "7";
        card.d = {4, 2};
        card.intensity = {100, 50};
        card.scale = .7912;
        state.candidates = {card};
        const auto source = std::make_shared<const ProductState>(state);
        auto encoded = infrastructure::legacy_fixture::encodeLegacyProject(source, {}, "frozen-clock");
        require(bool(encoded), "Canonical project encoding failed");
        auto decoded = infrastructure::legacy_fixture::decodeProject(encoded.value(), SearchBackend::cpp, {});
        require(bool(decoded), "Project round-trip failed");
        require(decoded.value().projectEnvelope == state.projectEnvelope,
                "Unknown envelope lost integer/null/array types");
        require(decoded.value().projectMetadata.at("unknown") == state.projectMetadata.at("unknown"),
                "Unknown diffraction metadata changed");
        CardType original;
        original.setD({4, 2}, radiation::default_wavelength_angstrom);
        const auto angles = original.getTth();
        require(decoded.value().candidates[0].tth == std::vector<double>(angles.begin(), angles.end()),
                "Project card coordinates changed");
        require(decoded.value().candidates[0].scale == card.scale &&
                    !decoded.value().candidates[0].fomCalculated,
                "Unscored previous scale changed");
        auto invalid = encoded.value();
        auto dd = invalid["diffraction_data"].toObject();
        dd["y"] = QJsonArray{1};
        invalid["diffraction_data"] = dd;
        require(!infrastructure::legacy_fixture::decodeProject(invalid, SearchBackend::cpp, {}),
                "Malformed project published state");
        QTemporaryDir directory;
        require(directory.isValid(), "Project fixture directory");
        const auto filename = directory.path() + QString::fromUtf8("/项目.qxp");
        require(bool(infrastructure::legacy_fixture::writeProject(filename, encoded.value())),
                "Atomic project file write failed");
        auto read = infrastructure::legacy_fixture::readProject(filename, SearchBackend::cpp, {});
        require(bool(read) && read.value().projectEnvelope == state.projectEnvelope,
                "Project file IO changed metadata");
        ProductSearchSession session;
        const auto before = session.snapshot();
        require(!openProjectInput(session, FailedProject{}, "missing", {}) && session.snapshot() == before,
                "Failed project IO committed");
        require(
            bool(openProjectInput(session, infrastructure::legacy_fixture::ProjectFileReader{}, filename.toStdString(), {})),
            "Project reader port commit failed");
        auto v1 = encoded.value();
        v1["version"] = 1;
        auto old = infrastructure::legacy_fixture::decodeProject(v1, SearchBackend::cpp, {});
        require(bool(old), "Historical project rejected");
        require(!old.value().preprocessing.fwhm, "Old QXP must not invent an estimated FWHM");
        auto configured = state;
        FwhmConfiguration configuration;
        configuration.method = FwhmMethod::geometric;
        configuration.default_fwhm = AngularWidthDegrees{.08};
        configuration.minimum_sampling_density = 8;
        configuration.extensions["future"] = MetadataObject{{"wide", std::int64_t{9007199254740993LL}}, {"null", nullptr}};
        configured.preprocessing.fwhm = configuration;
        FwhmUsage usage;
        usage.width = configuration.default_fwhm; usage.origin = FwhmOrigin::geometric;
        usage.algorithm = {"qualpf.peaks.fwhm.geometric", 1}; usage.dataset = configured.pattern.id;
        usage.source_id = configured.pattern.source_id; usage.source_revision = 37;
        usage.minimum_sampling_density = 8; usage.width_count = 1; usage.current = true;
        usage.extensions["future_usage"] = "preserve";
        configured.preprocessing.fwhm_usage = configured.preprocessing.peak_fwhm_usage = usage;
        configured.preprocessing.extensions["future_preprocessing"] = MetadataArray{nullptr, std::int64_t{9007199254740993LL}};
        auto savedFwhm = infrastructure::legacy_fixture::encodeLegacyProject(std::make_shared<const ProductState>(configured), {}, "frozen-clock");
        require(bool(savedFwhm), "Geometric settings must have explicit serializable provenance");
        auto restoredFwhm = infrastructure::legacy_fixture::decodeProject(savedFwhm.value(), SearchBackend::cpp, {});
        require(bool(restoredFwhm) && restoredFwhm.value().preprocessing.fwhm == configured.preprocessing.fwhm,
                "FWHM configuration or unknown fields changed");
        require(!restoredFwhm.value().preprocessing.fwhm_usage->current &&
                    restoredFwhm.value().preprocessing.fwhm_usage->origin == FwhmOrigin::geometric &&
                    restoredFwhm.value().preprocessing.fwhm_usage->extensions == usage.extensions,
                "Restored width must be historical geometric evidence");
        auto savedAgain = infrastructure::legacy_fixture::encodeLegacyProject(std::make_shared<const ProductState>(restoredFwhm.value()), {}, "frozen-clock");
        require(bool(savedAgain) && savedAgain.value()["preprocessing"] == savedFwhm.value()["preprocessing"],
                "FWHM project round trip lost source/extension values");
        for (const auto &bad : {QJsonValue(0), QJsonValue(9), QJsonValue(2.5), QJsonValue("6")}) {
            auto broken = savedFwhm.value(); auto pre = broken["preprocessing"].toObject(); auto block = pre["fwhm"].toObject();
            block["minimum_sampling_density"] = bad; pre["fwhm"] = block; broken["preprocessing"] = pre;
            require(!infrastructure::legacy_fixture::decodeProject(broken, SearchBackend::cpp, {}), "Malformed Q accepted");
        }
        for (const auto &[key, bad] : std::vector<std::pair<QString, QJsonValue>>{
                 {"version", 2}, {"method", "unknown"}, {"use_fixed", "true"},
                 {"default_fwhm", 0}, {"default_fwhm", 181}, {"default_fwhm", QJsonValue::Null}}) {
            auto broken = savedFwhm.value(); auto pre = broken["preprocessing"].toObject(); auto block = pre["fwhm"].toObject();
            block[key] = bad; pre["fwhm"] = block; broken["preprocessing"] = pre;
            require(!infrastructure::legacy_fixture::decodeProject(broken, SearchBackend::cpp, {}), "Malformed FWHM setting accepted");
        }
        auto fixedState = configured;
        fixedState.preprocessing.fwhm->use_fixed = true;
        auto &fixedUsage = *fixedState.preprocessing.fwhm_usage;
        fixedUsage.origin = FwhmOrigin::fixed; fixedUsage.algorithm = {"qualpf.fwhm.fixed", 1}; fixedUsage.width_count = 0;
        auto fixedProject = infrastructure::legacy_fixture::encodeLegacyProject(std::make_shared<const ProductState>(fixedState), {}, "frozen-clock");
        require(bool(fixedProject), "Fixed FWHM encoding failed");
        auto fixedRestored = infrastructure::legacy_fixture::decodeProject(fixedProject.value(), SearchBackend::cpp, {});
        require(bool(fixedRestored) && fixedRestored.value().preprocessing.fwhm->use_fixed &&
                    fixedRestored.value().preprocessing.fwhm_usage->origin == FwhmOrigin::fixed,
                "Fixed FWHM project round trip changed the use policy");
        auto initialState = state;
        auto &pattern = initialState.pattern;
        pattern.two_theta.clear(); pattern.observed.clear(); pattern.calculation_range.reset();
        pattern.processing.background_subtracted = false;
        for (int i = 0; i <= 400; ++i) {
            const double x = 19. + .01 * i;
            pattern.two_theta.push_back(TwoThetaDegrees{x});
            pattern.observed.push_back(10. + 100. * std::exp(-.5 * std::pow((x - 21.) / .12, 2)));
        }
        pattern.background = SampledSignal{std::vector<double>(401, 10.), SampleWindow{0}};
        InitialPeakConfiguration initialConfiguration;
        initialConfiguration.options = {.123456789f, false, 20, 7};
        initialConfiguration.extensions["future"] = MetadataObject{{"wide", std::int64_t{9007199254740993LL}}, {"null", nullptr}};
        auto initialSource = std::make_shared<const ProductState>(initialState);
        auto initial = evaluateInitialPeakSearch(initialSource, initialConfiguration,
            {{}, NumericalPolicy::legacy_mixed_precision, "project-initial-test"});
        require(bool(initial) && bool(applyInitialPeakSearch(initialState, initial.value())), "Initial evidence preparation failed");
        auto savedInitial = infrastructure::legacy_fixture::encodeLegacyProject(std::make_shared<const ProductState>(initialState), {}, "frozen-clock");
        require(bool(savedInitial), "Initial configuration encoding failed");
        auto restoredInitial = infrastructure::legacy_fixture::decodeProject(savedInitial.value(), SearchBackend::cpp, {});
        require(bool(restoredInitial) && restoredInitial.value().preprocessing.initial_peaks == initialState.preprocessing.initial_peaks &&
            !restoredInitial.value().preprocessing.initial_peak_usage, "Initial configuration or runtime persistence boundary changed");
        for (const auto &[key, bad] : std::vector<std::pair<QString, QJsonValue>>{
                {"version", 2}, {"slope", -1}, {"slope", 201}, {"slope", QJsonValue::Null},
                {"automatic_window", "true"}, {"half_window", 1}, {"half_window", 101},
                {"half_window", 2.5}, {"degree", 2}, {"degree", 21}}) {
            auto broken = savedInitial.value(); auto pre = broken["preprocessing"].toObject();
            auto block = pre["initial_peaks"].toObject(); block[key] = bad;
            pre["initial_peaks"] = block; broken["preprocessing"] = pre;
            require(!infrastructure::legacy_fixture::decodeProject(broken, SearchBackend::cpp, {}), "Malformed initial configuration accepted");
        }
        auto oldInitial = savedInitial.value(); auto oldPre = oldInitial["preprocessing"].toObject();
        oldPre.remove("initial_peaks"); oldInitial["preprocessing"] = oldPre;
        auto oldRestored = infrastructure::legacy_fixture::decodeProject(oldInitial, SearchBackend::cpp, {});
        require(bool(oldRestored) && !oldRestored.value().preprocessing.initial_peaks &&
            oldRestored.value().preprocessing.initial_peaks.value_or(InitialPeakConfiguration{}).options == preprocessing::InitialPeakOptions{},
            "Old project initial defaults changed");
        std::cout << "PASS: project values, unknown metadata, file ports, card angles and independent "
                     "quantification parity\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
