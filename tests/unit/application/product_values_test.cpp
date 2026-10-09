#include "../../support/preprocessing_baseline_codec.h"
#include "gui/model/cardtype.h"
#include "gui/model/experimentalpeaks.h"
#include "gui/state/search_session.h"
#include "infrastructure/serialization/metadata_values.h"
#include "qualpf/application/product_values.h"
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>

int main() {
    using namespace qualpf;
    using namespace application; using namespace qualpf::gui;
    const auto check = [](bool ok) {
        if (!ok)
            throw std::runtime_error("portable transport changed");
    };
    QJsonObject original{{"unknown", QJsonArray{QJsonValue(QJsonValue::Null), true, 1.25,
                                                QString::fromUtf8("\xE7\x9F\xBF\xE7\x89\xA9")}},
                         {"wide_integer", QJsonValue(qint64(9007199254740993LL))},
                         {"nested", QJsonObject{{"zero", 0}, {"empty", QJsonArray{}}}}};
    const auto portable = infrastructure::metadataValue(original);
    check(infrastructure::jsonValue(portable).toObject() == original);
    check(std::get<std::int64_t>(std::get<MetadataObject>(portable.data).at("wide_integer").data) ==
          9007199254740993LL);
    check(infrastructure::metadataValue(QJsonValue::Undefined) !=
          infrastructure::metadataValue(QJsonValue::Null));
    // Compare access semantics against the original Qt projection, including
    // fractional numbers, missing values and integers outside binary64 precision.
    for (const auto &v : {QJsonValue(), QJsonValue(QJsonValue::Undefined), QJsonValue(true),
                          QJsonValue(1), QJsonValue(1.25), QJsonValue(-0.0),
                          QJsonValue(qint64(9007199254740993LL)), QJsonValue("text")}) {
        const auto pure=infrastructure::metadataValue(v);
        check(pure.toBool(true)==v.toBool(true));
        check(pure.toDouble(-3)==v.toDouble(-3));
        check(pure.toInt(-3)==v.toInt(-3));
        check(pure.toString()==v.toString().toStdString());
    }
    const auto pureObject=portable.toObject();
    check(std::holds_alternative<UndefinedValue>(portable["missing"].data));
    check(MetadataValue{std::int64_t{1}}!=MetadataValue{1.0});
    ProductPeaks peaks;
    peaks.tth = {-0.0, 1.0, std::numeric_limits<double>::min()};
    peaks.intensityOrig = {2, 3, 4};
    const auto copied = peaks;
    check(!copied.integratedIntensity);
    peaks.integratedIntensity.emplace();
    check(peaks.integratedIntensity && peaks.integratedIntensity->empty());
    check(std::bit_cast<std::uint64_t>(copied.tth.front()) == std::bit_cast<std::uint64_t>(-0.0));
    ProductCard card;
    card.id = "stable";
    card.scale = 1.125;
    card.fomCalculated = true;
    card.d = {2.5, 1.25};
    card.tth = {30, 60};
    card.intensity = {100, 25};
    auto snapshot = card;
    card.intensity[0] = 0;
    check(snapshot.intensity[0] == 100 && snapshot.scale == 1.125 && snapshot.fomCalculated);
    check(CardType(snapshot).productValue() == snapshot);
    check(ExperimentalPeaks::fromProductValue(copied).productValue() == copied);
    const auto transported = ExperimentalPeaks::fromProductValue(copied).productValue();
    check(std::bit_cast<std::uint64_t>(transported.tth.front()) == std::bit_cast<std::uint64_t>(-0.0));
    qualpf::application::Workspace sessionOwner(qEnvironmentVariable("QUALPF_SEARCH_BACKEND") == "legacy" ? qualpf::application::SearchBackend::legacy_fallback : qualpf::application::SearchBackend::cpp); SessionView session(sessionOwner.session());
    auto opening = session.snapshot();
    auto next = *opening;
    next.projectMetadata = portable;
    check(bool(session.commit(opening, next)));
    check(session.snapshot()->canonical == session.canonicalSession().snapshot());
    check(infrastructure::jsonValue(session.snapshot()->projectMetadata).toObject() == original && opening->revision == 0);
    auto canonical = session.canonicalSession().snapshot();
    auto pure = *canonical;
    pure.projectMetadata["from_core"] = true;
    check(bool(session.canonicalSession().commit(canonical, pure)));
    check(session.snapshot()->projectMetadata["from_core"].toBool());
    check(session.snapshot()->revision == 2);
    const auto equivalent = [&](const PreprocessingConfiguration &c) {
        check(qualpf::infrastructure::encodePreprocessing(c) == testing::encodePreprocessingBaseline(c));
        auto a=preprocessingValues(c), b=preprocessingValues(PreprocessingConfiguration{});
        auto ja=qualpf::infrastructure::encodePreprocessing(c), jb=qualpf::infrastructure::encodePreprocessing({});
        for(const auto *key:{"candidate_pool","candidate_failure","peaks","selection"}) {
            a.erase(key); b.erase(key); ja.remove(key); jb.remove(key);
        }
        check((a==b)==(ja==jb));
        check(bool(validatePreprocessing(c)) ==
              bool(qualpf::infrastructure::decodePreprocessing(testing::encodePreprocessingBaseline(c))));
    };
    PreprocessingConfiguration c;
    equivalent(c);
    c.background = preprocessing::BackgroundOptions{};
    for (int v : {-1, 0, 1, 36, 37, 100000}) {
        c.background->coefficients = v;
        equivalent(c);
    }
    c.background = preprocessing::BackgroundOptions{};
    for (int v : {-1, 0, 1, 100000, 100001}) {
        c.background->iterations = v;
        equivalent(c);
    }
    c.background.reset();
    c.peaks = preprocessing::PeakOptions{};
    for (float v : {-1.f, 0.f, 100.f, 101.f, std::numeric_limits<float>::infinity(),
                    std::numeric_limits<float>::quiet_NaN()}) {
        c.peaks->threshold_percent = v;
        equivalent(c);
    }
    c.peaks = preprocessing::PeakOptions{};
    for (int v : {-1, 0, 1, 100, 101}) {
        c.peaks->half_window = v;
        equivalent(c);
    }
    c.peaks.reset();
    c.selection = PeakSelection{};
    for (double v : {-1., 0., 180., 181., std::numeric_limits<double>::infinity()}) {
        c.selection->maximum = v;
        equivalent(c);
    }
    c.selection.reset();
    c.candidate_failure = "nonfinite_candidate_measurement";
    equivalent(c);
    c.candidate_failure = "other";
    equivalent(c);
    c.candidate_failure.reset();
    c.background_points = std::vector<preprocessing::BackgroundPoint>{{1, 2}, {0, 2}};
    equivalent(c);
    c.background_points = std::vector<preprocessing::BackgroundPoint>{{0, 2}, {1, 2}};
    equivalent(c);
    c = {};
    c.background = preprocessing::BackgroundOptions{};
    c.background->bruckner_range = AngularRange{TwoThetaDegrees{5}, TwoThetaDegrees{90}};
    c.smoothing = preprocessing::SmoothingOptions{};
    c.peaks = preprocessing::PeakOptions{};
    c.peaks->detection_range = AngularRange{TwoThetaDegrees{6}, TwoThetaDegrees{80}};
    c.selection = PeakSelection{};
    c.smoothing_applied = false;
    c.background_points = std::vector<preprocessing::BackgroundPoint>{{.25f, 10.f}, {.5f, 20.f}};
    c.background_filters = {{5., 90., 2, 3}};
    PeakSet pool;
    pool.dataset = {18446744073709551615ULL};
    pool.source_precision = SourcePrecision::float32_abi;
    pool.effective_wavelength = WavelengthAngstrom{1.5406};
    pool.intensity_scale = IntensityScale::counts;
    Peak row;
    row.id = {18446744073709551614ULL};
    row.two_theta = TwoThetaDegrees{20};
    row.intensity = 100;
    row.original_intensity = 120;
    row.d_spacing = DSpacingAngstrom{4};
    row.delta_d = DSpacingAngstrom{.02};
    row.fwhm = AngularWidthDegrees{.1};
    row.integrated_intensity = 10;
    pool.peaks = {row};
    c.peak_candidates = pool;
    equivalent(c);
    std::cout << "PASS: metadata, exact payload and independently owned product values\n";
}
