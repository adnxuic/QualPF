#include "classic_fixture.h"
#include "qualpf/preprocessing/algorithms.h"
#include "qualpf/search/algorithms.h"
#include "qualpf/search/rir.h"
#include "../../../src/core/peaks/legacy_peak_measurement.h"
#include <cfenv>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <limits>
#include <source_location>
#include <stdexcept>
using namespace qualpf;
static void check(bool value, std::source_location at = std::source_location::current()) {
    if (!value)
        throw std::runtime_error("Core module contract at line " + std::to_string(at.line()));
}
static AnalysisSpectrum synthetic_spectrum(Pattern pattern) {
    pattern.processing.background_subtracted = false;
    pattern.processing.intensity_scaled = false;
    pattern.background = SampledSignal{std::vector<double>(pattern.observed.size(), 0), SampleWindow{0}};
    return AnalysisSpectrum::create(std::move(pattern)).value();
}
struct Initial : preprocessing::IInitialPeakFinder {
    mutable int calls = 0;
    AlgorithmIdentity identity() const override { return {"test.initial", 1}; }
    Result<preprocessing::InitialPeakResult> find(const AnalysisSpectrum &spectrum, WavelengthAngstrom,
        const preprocessing::InitialPeakOptions &, const ExecutionContext &) const override {
        const auto &pattern = spectrum.pattern();
        ++calls;
        preprocessing::InitialPeakResult result;
        result.dataset = pattern.id;
        result.candidates = {{1}, {2}, {3}, {4}};
        return Result<preprocessing::InitialPeakResult>{result};
    }
};
struct Fwhm : preprocessing::IFwhmEstimator {
    mutable int calls = 0;
    AlgorithmIdentity identity() const override { return {"test.calibration", 1}; }
    Result<preprocessing::FwhmEstimationResult> estimate(const AnalysisSpectrum &spectrum, const preprocessing::InitialPeakResult &initial, const ExecutionContext &) const override {
        const auto &pattern = spectrum.pattern();
        ++calls;
        check(initial.candidates.size() == 4 && initial.dataset == pattern.id && pattern.observed[2] == 200);
        return Result<preprocessing::FwhmEstimationResult>{preprocessing::FwhmEstimationResult{
            {preprocessing::FwhmEstimate::State::measured, AngularWidthDegrees{((.4f + .3f) + .2f) / 3.f}, 3}, {}}};
    }
};
struct Extractor : preprocessing::IExperimentalPeakExtractor {
    mutable int calls = 0;
    mutable float received_range = 0;
    AlgorithmIdentity identity() const override { return {"test.extractor", 1}; }
    Result<preprocessing::PeakSearchResult> extract(const AnalysisSpectrum &spectrum, const preprocessing::PeakCalibration &cal,
                                                    WavelengthAngstrom, AngularWidthDegrees delta,
                                                    const ExecutionContext &) const override {
        ++calls;
        received_range = cal.applied.search_range;
        check(delta.value == .25); // Matching tolerance is independent of calibrated range.
        preprocessing::PeakSearchResult result;
        result.applied = cal.applied;
        result.automatic_tolerance = .375;
        return Result<preprocessing::PeakSearchResult>{result};
    }
};
struct Matcher : search::IClassicMatcher {
    mutable int calls = 0;
    bool incompatible = false;
    AlgorithmIdentity identity() const override { return {"test.matcher", 1}; }
    AlgorithmIdentity produces() const override {
        return incompatible ? AlgorithmIdentity{"test.incompatible", 1} : search::classic_match_contract();
    }
    Result<search::ClassicMatch> match(const search::ClassicMatchInput &input,
                                       const ExecutionContext &context) const override {
        ++calls;
        auto result = search::ClassicMatcher{}.match(input, context);
        if (result)
            result.value().position_difference_sum = .375f;
        return result;
    }
};
static void candidateMeasurements() {
    using namespace preprocessing;
    using detail::Measured;
    const std::vector<float> x{20, 21, 22};
    std::vector<Measured> peaks{{21, 3, 1}};
    auto measured = detail::measure_peak_candidates(peaks, x, std::vector<float>{1, 3, 1},
                                                    PeakDetectionStage::initial);
    // Independent triangle: height 3-1=2; trapezoids (1+3)/2 + (3+1)/2=4.
    check(measured && measured.value().empty() && peaks.size() == 1);
    check(peaks[0].y == 2 && peaks[0].area == 4 && peaks[0].width == 1);
    peaks = {{21, 2, 1}};
    measured = detail::measure_peak_candidates(peaks, x, std::vector<float>{2, 2, 2},
                                               PeakDetectionStage::formal);
    check(measured && peaks[0].y == 0 && peaks[0].area == 2); // Flat baseline; unchanged tie order.
    for (const auto &[signal, candidate] : std::vector<std::pair<std::vector<float>, Measured>>{
             {{3, 1, 3}, {21, 1, 1}}, {{1, 2, 3}, {20, 1, 1}},
             {{3, 2, 1}, {22, 1, 1}}, {{1, 3, 1}, {21, 3, .1f}}}) {
        peaks = {candidate};
        std::feclearexcept(FE_ALL_EXCEPT);
        measured = detail::measure_peak_candidates(peaks, x, signal, PeakDetectionStage::formal);
        check(!measured && measured.error().reason == "coincident_baseline_endpoints" && peaks.empty());
        check(!(std::fetestexcept(FE_DIVBYZERO | FE_INVALID)));
    }
    const std::vector<float> mixedX{20, 21, 22, 23, 24, 25, 26};
    const std::vector<float> mixedY{3, 1, 3, 1, 5, 1, 2};
    peaks = {{21, 1, 1}, {24, 5, .5f}};
    std::feclearexcept(FE_ALL_EXCEPT);
    measured = detail::measure_peak_candidates(peaks, mixedX, mixedY, PeakDetectionStage::formal, 100);
    check(measured && measured.value().size() == 1 && peaks.size() == 1);
    check(!(std::fetestexcept(FE_DIVBYZERO | FE_INVALID)));
    const auto &r = measured.value().front();
    check(r.stage == PeakDetectionStage::formal && r.candidate_index == 0 && r.sample_index == 101 &&
          r.two_theta.value == 21 && r.reason == "coincident_baseline_endpoints");
    check(peaks[0].x == 24 && peaks[0].y == 4 && peaks[0].area == 6 && peaks[0].width == .5f);
    // A rejected narrow-window candidate has raw height 10 and overlaps the
    // following height-5 candidate. It must not move that candidate's baseline.
    peaks = {{21, 10, .1f}, {22, 5, 1}};
    measured = detail::measure_peak_candidates(peaks, std::vector<float>{20, 21, 22, 23, 24},
        std::vector<float>{1, 10, 5, 1, 2}, PeakDetectionStage::formal);
    check(measured && measured.value().size() == 1 && peaks.size() == 1);
    check(peaks[0].x == 22 && peaks[0].y == 4 && peaks[0].area == 16);
    peaks.clear();
    measured = detail::measure_peak_candidates(peaks, x, mixedY, PeakDetectionStage::initial);
    check(measured && measured.value().empty()); // Genuine absence is distinct from all rejected.
}
static void refinementAndReduction() {
    using namespace preprocessing;
    std::vector<float> x, y;
    constexpr float sigma = .12f;
    for (int i = 0; i <= 200; ++i) {
        const float angle = float(20. + .01 * i);
        x.push_back(angle);
        const double z = (double(angle) - 21.) / double(sigma);
        y.push_back(float(100. * std::exp(-.5 * z * z)));
    }
    PeakOptions options;
    std::vector<RejectedPeakCandidate> rejected;
    // Different derivative triggers locate the same maximum. Fits still precede
    // duplicate suppression, while only one measured peak survives.
    const std::vector<std::size_t> triggers{99, 100, 101};
    auto refined = detail::refine_peak_candidates(x, y, options, triggers,
        PeakDetectionStage::initial, rejected);
    check(refined && refined.value().first.size() == 1 && refined.value().second.size() == 1 && rejected.empty());
    const auto &peak = refined.value().first.front();
    const double analyticWidth = 2. * std::sqrt(2. * std::log(2.)) * double(sigma);
    // Existing frozen width tolerance: absolute 1e-5 plus relative 1e-5.
    check(peak.x == 21 && std::abs(peak.width - analyticWidth) <= 1e-5 + 1e-5 * analyticWidth);
    options.threshold_percent = 101;
    refined = detail::refine_peak_candidates(x, y, options, triggers, PeakDetectionStage::initial, rejected);
    check(refined && refined.value().first.empty() && refined.value().second.size() == 1);
    options.threshold_percent = 1000000;
    refined = detail::refine_peak_candidates(x, y, options, triggers, PeakDetectionStage::initial, rejected);
    check(refined && refined.value().first.empty() && refined.value().second.empty());
    options = {};
    refined = detail::refine_peak_candidates(std::vector<float>{20, 21, 22},
        std::vector<float>{1, 1, 1}, options, std::vector<std::size_t>{1},
        PeakDetectionStage::initial, rejected);
    check(!refined && refined.error().reason == "invalid_fitted_width"); // log(1)=0: exactly zero curvature.

    // A central dip gives positive log curvature in the small trial windows;
    // the original longer retry sequence recovers the surrounding Gaussian.
    y[100] *= .25f;
    options.half_window = 1;
    refined = detail::refine_peak_candidates(x, y, options, std::vector<std::size_t>{100},
        PeakDetectionStage::initial, rejected);
    check(refined && refined.value().first.empty());
    options.half_window = 11;
    refined = detail::refine_peak_candidates(x, y, options, std::vector<std::size_t>{100},
        PeakDetectionStage::initial, rejected);
    check(refined && refined.value().first.size() == 1);

    // A taller nearby maximum is kept by the width-dependent adjacency rule.
    y[100] *= 4; y[104] = 110;
    options.half_window = 1;
    refined = detail::refine_peak_candidates(x, y, options, std::vector<std::size_t>{100, 104},
        PeakDetectionStage::initial, rejected);
    check(refined && refined.value().first.size() == 1 && refined.value().first[0].x == x[104]);

    x.clear(); y.clear(); options = {};
    for (int i = 0; i <= 4000; ++i) {
        const float angle = float(20. + .01 * i);
        x.push_back(angle);
        double height = .001;
        for (double center : {25., 35., 45.}) height += 100. * std::exp(-.5 * std::pow((angle - center) / 2., 2));
        y.push_back(float(height));
    }
    refined = detail::refine_peak_candidates(x, y, options, std::vector<std::size_t>{500, 1500, 2500},
        PeakDetectionStage::initial, rejected);
    check(refined && refined.value().first.size() == 3);
    check(refined.value().first[0].width == .15f * 1.1f && refined.value().first[1].width == .2f &&
          refined.value().first[2].width == .15f * 1.1f); // Preserve the historical previous-width reduction.

    std::vector<Peak> measured;
    for (auto [height, width] : {std::pair{1., .8}, {4., .4}, {3., .3}, {2., .2}}) {
        Peak p; p.intensity = height; p.fwhm = AngularWidthDegrees{width}; measured.push_back(p);
    }
    auto strongest = detail::summarize_fwhm(measured);
    check(strongest && strongest.value().peak_count == 3 &&
          strongest.value().width->value == double(((.4f + .3f) + .2f) / 3.f));
    measured.clear();
    for (double width : {.1, .2, .4, .8}) {
        Peak p; p.intensity = 1; p.fwhm = AngularWidthDegrees{width}; measured.push_back(p);
    }
    auto width = detail::summarize_fwhm(measured);
    check(width && width.value().peak_count == 3 &&
          width.value().width->value == double(((.1f + .2f) + .4f) / 3.f));
    measured.resize(2);
    width = detail::summarize_fwhm(measured);
    check(width && width.value().peak_count == 2 && width.value().width->value == double((.1f + .2f) / 2.f));
    measured.resize(1); measured[0].fwhm = AngularWidthDegrees{0};
    width = detail::summarize_fwhm(measured);
    check(width && width.value().state == FwhmEstimate::State::measured && width.value().width->value == 0);
    for (double invalid : {-1., std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        measured[0].fwhm = AngularWidthDegrees{invalid};
        check(!detail::summarize_fwhm(measured));
    }
    measured[0].fwhm.reset();
    check(!detail::summarize_fwhm(measured));
    measured[0].fwhm = AngularWidthDegrees{.1}; measured[0].intensity = std::numeric_limits<double>::quiet_NaN();
    check(!detail::summarize_fwhm(measured));
    check(detail::summarize_fwhm({}).value().state == FwhmEstimate::State::no_initial_peaks);
}
static void spectrumWidths() {
    using namespace preprocessing;
    testing::ClassicFixture f;
    Pattern pattern; pattern.id = {1};
    for (int i = 0; i <= 200; ++i) pattern.two_theta.push_back(TwoThetaDegrees{20. + .01 * i});
    InitialPeakResult initial;
    initial.dataset = pattern.id; initial.candidates = {{100}};
    // Resolve curvature across enough samples for both widths using the existing
    // binary32 fit and frozen tolerance, rather than loosening the assertion.
    initial.applied.half_window = 30;
    double previous = 0;
    for (double sigma : {.12, .24}) {
        pattern.observed.clear();
        for (const auto angle : pattern.two_theta) {
            const double z = (double(float(angle.value)) - 21.) / sigma;
            pattern.observed.push_back(100. * std::exp(-.5 * z * z));
        }
        auto width = ClassicFwhmEstimator{}.estimate(synthetic_spectrum(pattern), initial, f.context);
        check(width && width.value().estimate.peak_count == 1);
        const double expected = 2. * std::sqrt(2. * std::log(2.)) * sigma;
        if (std::abs(width.value().estimate.width->value - expected) > 1e-5 + 1e-5 * expected)
            std::cerr << "Analytic Gaussian sigma=" << sigma << ": expected " << expected
                      << ", fitted " << width.value().estimate.width->value << '\n';
        check(std::abs(width.value().estimate.width->value - expected) <= 1e-5 + 1e-5 * expected);
        check(width.value().estimate.width->value > previous);
        previous = width.value().estimate.width->value;
    }
    auto found = ClassicInitialPeakFinder{}.find(synthetic_spectrum(pattern), WavelengthAngstrom{1.54}, f.context);
    check(found && !found.value().candidates.empty() && validate_initial_peak_candidates(found.value(), pattern));
    const auto gaussianSignal = pattern.observed;
    // SG ringing on a step yields triggers in a constant segment: detection is
    // valid, but log(1)=0 has no finite Gaussian width. Only the FWHM stage fails.
    for (std::size_t i = 0; i < pattern.observed.size(); ++i) pattern.observed[i] = i < 100 ? 1 : 2;
    found = ClassicInitialPeakFinder{}.find(synthetic_spectrum(pattern), WavelengthAngstrom{1.54}, f.context);
    check(found && !found.value().candidates.empty());
    auto failed = ClassicFwhmEstimator{}.estimate(synthetic_spectrum(pattern), found.value(), f.context);
    check(!failed && failed.error().reason == "invalid_fitted_width");
    // A nonpositive sample makes every Gaussian trial fail normally; this is a
    // usable empty result, not an invented width or an all-unmeasurable failure.
    pattern.observed.assign(pattern.two_theta.size(), 1); pattern.observed[100] = 0;
    auto empty = ClassicFwhmEstimator{}.estimate(synthetic_spectrum(pattern), initial, f.context);
    check(empty && empty.value().estimate.state == FwhmEstimate::State::no_initial_peaks &&
          !empty.value().estimate.width && empty.value().rejected_candidates.empty());
    pattern.observed = gaussianSignal;
    initial.applied.search_range = .00001f; initial.applied.threshold_percent = 100;
    empty = ClassicFwhmEstimator{}.estimate(synthetic_spectrum(pattern), initial, f.context);
    check(empty && empty.value().estimate.state == FwhmEstimate::State::no_initial_peaks);
    PeakOptions automatic; automatic.search_range = -1;
    auto fallback = prepare_classic_peak_parameters(automatic, empty.value().estimate, WavelengthAngstrom{1.54}, f.context);
    check(fallback && fallback.value().source == PeakCalibration::Source::wavelength_seed &&
          !fallback.value().estimated_fwhm);
}
static void initialOptions() {
    using namespace preprocessing;
    Pattern p; p.id = {45};
    for (int i = 0; i <= 400; ++i) {
        const double x = 19. + .01 * i;
        p.two_theta.push_back(TwoThetaDegrees{x});
        p.observed.push_back(100. * std::exp(-.5 * std::pow((x - 21.) / .12, 2)));
    }
    testing::ClassicFixture f;
    const auto original = ClassicInitialPeakFinder{}.find(synthetic_spectrum(p), WavelengthAngstrom{1.54056}, f.context);
    auto found = ClassicInitialPeakFinder{}.find(synthetic_spectrum(p), WavelengthAngstrom{1.54056}, InitialPeakOptions{}, f.context);
    check(original && found && original.value().candidates.size() == found.value().candidates.size());
    for (std::size_t i = 0; i < found.value().candidates.size(); ++i)
        check(original.value().candidates[i].sample_index == found.value().candidates[i].sample_index);
    check(found.value().applied.slope == .1f && found.value().applied.polynomial_degree == 9);
    InitialPeakOptions options{0.f, false, 20, 9};
    found = ClassicInitialPeakFinder{}.find(synthetic_spectrum(p), WavelengthAngstrom{1.54056}, options, f.context);
    check(found && found.value().applied.half_window == 20 && !found.value().applied.automatic_window);
    check(std::any_of(found.value().candidates.begin(), found.value().candidates.end(), [](auto c) {
        return c.sample_index >= 198 && c.sample_index <= 202; // Analytic Gaussian apex is sample 200.
    }));
    options.slope = 200;
    found = ClassicInitialPeakFinder{}.find(synthetic_spectrum(p), WavelengthAngstrom{1.54056}, options, f.context);
    check(found && found.value().candidates.empty()); // Normalized derivative jump cannot exceed 200.
    options = {0.f, false, 2, 20};
    check(bool(ClassicInitialPeakFinder{}.find(synthetic_spectrum(p), WavelengthAngstrom{1.54056}, options, f.context))); // Effective degree is 4.
    for (const InitialPeakOptions bad : {InitialPeakOptions{-1.f}, InitialPeakOptions{201.f},
            InitialPeakOptions{std::numeric_limits<float>::quiet_NaN()}, InitialPeakOptions{.1f, false, 1, 9},
            InitialPeakOptions{.1f, false, 101, 9}, InitialPeakOptions{.1f, true, 11, 2},
            InitialPeakOptions{.1f, true, 11, 21}}) {
        auto rejected = ClassicInitialPeakFinder{}.find(synthetic_spectrum(p), WavelengthAngstrom{1.54056}, bad, f.context);
        check(!rejected && rejected.error().reason == "invalid_initial_peak_options");
    }
    p.two_theta.resize(20); p.observed.assign(20, 1.); // Reach window admission with a positive normalization.
    auto shortInput = ClassicInitialPeakFinder{}.find(synthetic_spectrum(p), WavelengthAngstrom{1.54056}, {.1f, false, 11, 9}, f.context);
    check(!shortInput && shortInput.error().field == "smoothing.sg");
}
static void spectrumInputs() {
    testing::ClassicFixture f; auto p = f.pattern;
    p.background->values.assign(p.observed.size(), 5);
    auto raw = AnalysisSpectrum::create(p);
    check(raw && raw.value().pattern().observed == p.observed && raw.value().pattern().background->values[0] == 5);
    p.processing.background_subtracted = true;
    auto rejected = AnalysisSpectrum::create(p);
    check(!rejected && rejected.error().reason == "subtracted_input_refused");
    p.processing.background_subtracted.reset(); rejected = AnalysisSpectrum::create(p);
    check(!rejected && rejected.error().reason == "subtraction_state_unknown");
    p.processing.background_subtracted = false; p.background->window->first = 1;
    check(!AnalysisSpectrum::create(p));
    p = f.pattern; p.background->values = p.observed; p.processing.intensity_scaled = true;
    const auto zeroNet = AnalysisSpectrum::create(p).value();
    auto unsupported = f.context; unsupported.numerical_policy = NumericalPolicy::explicit_float64;
    const auto policy = search::ClassicMatcher{}.match({zeroNet,f.peaks,f.card,f.parameters}, unsupported);
    check(!policy && policy.error().code == ErrorCode::unsupported && policy.error().field == "numerical_policy");
}
int main() try {
    spectrumInputs();
    initialOptions();
    candidateMeasurements();
    refinementAndReduction();
    spectrumWidths();
    testing::ClassicFixture f;
    Initial initial;
    Fwhm calibration;
    Extractor extractor;
    preprocessing::PeakOptions options;
    options.search_range = -1;
    auto r = preprocessing::extract_experimental_peaks(synthetic_spectrum(f.pattern), options, WavelengthAngstrom{1.54},
                                                       AngularWidthDegrees{.25}, initial, calibration,
                                                       extractor, f.context);
    check(r && initial.calls == 1 && calibration.calls == 1 && extractor.calls == 1);
    const float sum = (.4f + .3f) + .2f;
    check(extractor.received_range == (sum / 3.f) * .9f);
    check(r.value().calibration->estimated_fwhm->value == double(sum / 3.f));
    check(r.value().calibration->width_count == 3 && r.value().automatic_tolerance == .375);
    options.search_range = .2f;
    r = preprocessing::extract_experimental_peaks(synthetic_spectrum(f.pattern), options, WavelengthAngstrom{1.54},
                                                  AngularWidthDegrees{.25}, initial, calibration, extractor,
                                                  f.context);
    check(r && initial.calls == 1 && calibration.calls == 1 && extractor.calls == 2);
    check(r.value().calibration->source == preprocessing::PeakCalibration::Source::manual_parameters);
    preprocessing::InitialPeakResult empty;
    empty.dataset = f.pattern.id;
    options.search_range = -1;
    auto emptyWidth = preprocessing::ClassicFwhmEstimator{}.estimate(synthetic_spectrum(f.pattern), empty, f.context);
    check(emptyWidth && emptyWidth.value().estimate.state == preprocessing::FwhmEstimate::State::no_initial_peaks);
    auto fallback = preprocessing::prepare_classic_peak_parameters(options, emptyWidth.value().estimate, WavelengthAngstrom{1.54}, f.context);
    const float w = 1.54f;
    const float seed = .15f * std::sqrt((64.f - 1.54056f * 1.54056f) / (64.f - w * w)) * (w / 1.54056f);
    check(fallback && fallback.value().applied.search_range == seed && !fallback.value().estimated_fwhm);
    const preprocessing::FwhmEstimate zeroWidth{preprocessing::FwhmEstimate::State::measured, AngularWidthDegrees{0}, 1};
    auto zero = preprocessing::prepare_classic_peak_parameters(options, zeroWidth, WavelengthAngstrom{1.54}, f.context);
    check(zero && zero.value().formal_search_range.value == 0 && zero.value().estimated_fwhm->value == 0);
    for (double invalid : {-1., std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()}) {
        auto evidence = zeroWidth;
        evidence.width = AngularWidthDegrees{invalid};
        check(!preprocessing::prepare_classic_peak_parameters(options, evidence, WavelengthAngstrom{1.54}, f.context));
    }
    auto inconsistent = emptyWidth.value().estimate;
    inconsistent.width = AngularWidthDegrees{0};
    check(!preprocessing::validate_fwhm_estimate(inconsistent));
    options.search_range = .2f;
    check(preprocessing::prepare_classic_peak_parameters(options, inconsistent, WavelengthAngstrom{1.54}, f.context)
              .value().source == preprocessing::PeakCalibration::Source::manual_parameters);

    const auto matchSpectrum = synthetic_spectrum(f.pattern);
    search::ClassicMatchInput matchInput{matchSpectrum, f.peaks, f.card, f.parameters};
    Matcher matcher;
    auto score = search::score_classic(matchInput, matcher, f.context);
    check(score && matcher.calls == 1 && score.value().score.position == .5);
    check(score.value().score.total < 1 && score.value().score.total > .9);
    matcher.incompatible = true;
    score = search::score_classic(matchInput, matcher, f.context);
    check(!score && score.error().reason == "incompatible_match_contract" && matcher.calls == 1);

    const std::vector<double> d{3, 2, 1}, delta{.01, .01, .01};
    const std::vector<DSpacingAngstrom> top{DSpacingAngstrom{3}, DSpacingAngstrom{2}};
    check(search::passes_stored_strongest(d, delta, top, f.context).value());
    check(!search::accepts_record_quality(" D ", false, f.context).value());
    const std::vector<double> heights{10};
    const std::vector<search::RirInput> phases{{true, heights, 2, 2}, {true, heights, 3, 1}};
    check(search::quantify_rir(phases) == std::vector<double>({25, 75}));
    std::cout << "PASS: staged replacement, no repeated initialization, typed width provenance, "
                 "evidence-consuming FOM and compatibility rejection\n";
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
}
