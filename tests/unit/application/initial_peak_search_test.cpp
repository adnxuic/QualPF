#include "qualpf/application/initial_peak_search.h"
#include "qualpf/application/configured_fwhm.h"
#include "qualpf/application/preprocessing_workflows.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
static void check(bool ok, std::source_location at = std::source_location::current()) {
    if (!ok) throw std::runtime_error("Initial peak search at line " + std::to_string(at.line()));
}
struct Initial final : preprocessing::IInitialPeakFinder {
    mutable int calls = 0;
    mutable preprocessing::InitialPeakOptions received;
    mutable std::vector<double> signal;
    bool malformed = false;
    AlgorithmIdentity selected{"qualpf.peaks.initial.cpp", 5};
    AlgorithmIdentity identity() const override { return selected; }
    Result<preprocessing::InitialPeakResult> find(const AnalysisSpectrum &spectrum, WavelengthAngstrom w,
        const preprocessing::InitialPeakOptions &o, const ExecutionContext &c) const override {
        const auto &p = spectrum.pattern();
        ++calls; received = o; signal = p.observed;
        auto found = preprocessing::ClassicInitialPeakFinder{}.find(spectrum, w, o, c);
        if (found && malformed) found.value().dataset = DatasetId{999};
        return found;
    }
};
struct Width final : preprocessing::IFwhmEstimator {
    mutable int calls = 0;
    AlgorithmIdentity identity() const override { return {"qualpf.peaks.fwhm.cpp", 3}; }
    Result<preprocessing::FwhmEstimationResult> estimate(const AnalysisSpectrum &spectrum, const preprocessing::InitialPeakResult &,
        const ExecutionContext &) const override {
        ++calls;
        preprocessing::FwhmEstimationResult r;
        r.estimate = {preprocessing::FwhmEstimate::State::measured, AngularWidthDegrees{.2}, 1};
        return Result<preprocessing::FwhmEstimationResult>{r};
    }
};
struct Background final : preprocessing::IBackgroundEstimator {
    mutable int calls = 0;
    AlgorithmIdentity identity() const override { return {"qualpf.background.cpp", 1}; }
    Result<preprocessing::BackgroundResult> estimate(const Pattern &p, const preprocessing::BackgroundOptions &,
        WavelengthAngstrom, std::optional<std::span<const preprocessing::BackgroundPoint>>,
        const ExecutionContext &) const override {
        ++calls;
        preprocessing::BackgroundResult r; r.values.assign(p.observed.size(), 10.);
        return Result<preprocessing::BackgroundResult>{r};
    }
};
struct Formal final : preprocessing::IExperimentalPeakExtractor {
    mutable int calls = 0;
    AlgorithmIdentity identity() const override { return {"qualpf.peaks.experimental.cpp", 3}; }
    Result<preprocessing::PeakSearchResult> extract(const AnalysisSpectrum &spectrum, const preprocessing::PeakCalibration &,
        WavelengthAngstrom, AngularWidthDegrees, const ExecutionContext &) const override {
        ++calls;
        return Result<preprocessing::PeakSearchResult>{Error{ErrorCode::unsupported, "test", "Formal extraction must not run"}};
    }
};
static ProductState gaussian() {
    ProductState s; auto &p = s.pattern;
    p.id = {91}; p.source_id = "analytic-gaussian";
    p.radiation.lines = {{WavelengthAngstrom{1.54056}, 1}};
    p.processing.background_subtracted = false;
    for (int i = 0; i <= 400; ++i) {
        const double x = 19. + .01 * i;
        p.two_theta.push_back(TwoThetaDegrees{x});
        p.observed.push_back(10. + 100. * std::exp(-.5 * std::pow((x - 21.) / .12, 2)));
    }
    p.background = SampledSignal{std::vector<double>(p.observed.size(), 10.), SampleWindow{0}};
    return s;
}
int main() try {
    auto initial = std::make_shared<Initial>(); auto width = std::make_shared<Width>();
    auto background = std::make_shared<Background>(); auto formal = std::make_shared<Formal>();
    AlgorithmModules modules; modules.initial_peaks = initial; modules.fwhm = width; modules.background = background;
    modules.experimental_peaks = formal;
    auto algorithms = resolveAlgorithms(AlgorithmSpec{}, modules); check(bool(algorithms));
    auto state = gaussian(); state.algorithms = algorithms.value();
    auto source = std::make_shared<const ProductState>(state);
    auto defaults = evaluateInitialPeakSearch(source, {}, {{}, NumericalPolicy::legacy_mixed_precision, "initial-defaults"});
    check(bool(defaults)); auto defaultNext = *source;
    check(bool(applyInitialPeakSearch(defaultNext, defaults.value())));
    check(preprocessingValues(defaultNext.preprocessing) == preprocessingValues(source->preprocessing));
    initial->calls = 0;
    InitialPeakConfiguration configuration; configuration.options = {.125f, false, 20, 7};
    configuration.extensions["future"] = MetadataObject{{"integer", std::int64_t{9007199254740993LL}}, {"null", nullptr}};
    ExecutionContext context{{}, NumericalPolicy::legacy_mixed_precision, "initial-test"};
    auto found = evaluateInitialPeakSearch(source, configuration, context);
    check(found && !found.value().usage->detection.candidates.empty());
    check(initial->calls == 1 && width->calls == 0 && formal->calls == 0 && background->calls == 0);
    check(initial->received == configuration.options && source->pattern.observed == state.pattern.observed);
    for (std::size_t i = 0; i < initial->signal.size(); ++i)
        check(initial->signal[i] == state.pattern.observed[i]);
    auto next = *source; check(bool(applyInitialPeakSearch(next, found.value())));
    check(next.peaks.tth == source->peaks.tth && next.pattern.observed == source->pattern.observed &&
        next.pattern.background->values == source->pattern.background->values && !next.preprocessing.fwhm_usage);
    ProductSearchSession session(state); source = session.snapshot();
    found = evaluateInitialPeakSearch(source, configuration, context); check(bool(found));
    next = *source; check(bool(applyInitialPeakSearch(next, found.value()))); check(bool(session.commit(source, next)));
    auto evidence = session.snapshot()->preprocessing.initial_peak_usage;
    source = session.snapshot(); next = *source; next.projectMetadata["unrelated"] = true;
    check(bool(session.commit(source, next))); check(session.snapshot()->preprocessing.initial_peak_usage == evidence);
    source = session.snapshot(); next = *source; next.pattern.observed[0] += 1;
    check(bool(session.commit(source, next))); check(!session.snapshot()->preprocessing.initial_peak_usage);
    auto missing = state; missing.pattern.background.reset();
    auto privateBackground = evaluateInitialPeakSearch(std::make_shared<const ProductState>(missing), configuration, context);
    check(privateBackground && background->calls == 1 && !missing.pattern.background);
    check(privateBackground.value().usage->spectrum->pattern().observed == initial->signal);
    auto unknown = missing; unknown.pattern.processing.background_subtracted.reset();
    auto rejected = evaluateInitialPeakSearch(std::make_shared<const ProductState>(unknown), configuration, context);
    check(!rejected && rejected.error().reason == "subtraction_state_unknown");
    std::stop_source cancellation; cancellation.request_stop();
    auto cancelled = context; cancelled.cancellation = cancellation.get_token();
    auto stopped = evaluateInitialPeakSearch(std::make_shared<const ProductState>(state), configuration, cancelled);
    check(!stopped && stopped.error().code == ErrorCode::cancelled);
    initial->malformed = true;
    rejected = evaluateInitialPeakSearch(std::make_shared<const ProductState>(state), configuration, context);
    check(!rejected && rejected.error().reason == "invalid_algorithm_output"); initial->malformed = false;
    auto emptyOptions = configuration; emptyOptions.options.slope = 200;
    auto empty = evaluateInitialPeakSearch(std::make_shared<const ProductState>(state), emptyOptions, context);
    check(empty && empty.value().usage->detection.candidates.empty());
    auto configured = state; configured.preprocessing.initial_peaks = configuration;
    auto configuredSource = std::make_shared<const ProductState>(configured);
    auto fwhm = evaluateConfiguredFwhm(configuredSource, {configured.pattern, FwhmConfiguration{}}, context);
    check(fwhm && fwhm.value().usage && initial->received == configuration.options && width->calls == 1);
    check(fwhm.value().usage->initial_peak_options == configuration.options);
    CommonPreprocessingRequest commonRequest;
    commonRequest.peaks = preprocessing::PeakOptions{}; commonRequest.peaks->search_range = -1;
    commonRequest.purpose = AnalysisPurpose::peak_measurement;
    commonRequest.wavelength = configured.pattern.radiation.lines.front().wavelength;
    auto common = evaluateCommonPreprocessing(configuredSource, commonRequest, context);
    check(common && common.value().initial_peaks && initial->received == configuration.options && formal->calls == 0);
    commonRequest.initial_peaks = preprocessing::InitialPeakOptions{.25f, false, 15, 5};
    common = evaluateCommonPreprocessing(configuredSource, commonRequest, context);
    check(common && initial->received == *commonRequest.initial_peaks && configuredSource->preprocessing.initial_peaks == configuration);
    configured.preprocessing.fwhm = fwhm.value().configuration;
    configured.preprocessing.fwhm_usage = fwhm.value().usage;
    configured.preprocessing.peak_fwhm_usage = fwhm.value().usage;
    ProductSearchSession widths(configured); auto opening = widths.snapshot(); next = *opening;
    next.preprocessing.initial_peaks->options.slope = .25f;
    check(bool(widths.commit(opening, next)));
    check(!widths.snapshot()->preprocessing.fwhm_usage->current && widths.snapshot()->preprocessing.peak_fwhm_usage->current);
    check(widths.snapshot()->peaks.tth == configured.peaks.tth);
    auto rotated = *session.snapshot(); rotated.preprocessing.initial_peaks = configuration;
    auto rotatedSource = std::make_shared<const ProductState>(rotated);
    found = evaluateInitialPeakSearch(rotatedSource, configuration, context); check(bool(found));
    check(bool(applyInitialPeakSearch(rotated, found.value())));
    ProductSearchSession wavelengths(rotated); opening = wavelengths.snapshot(); next = *opening;
    next.pattern.radiation.lines[0].wavelength = WavelengthAngstrom{1.55};
    check(bool(wavelengths.commit(opening, next))); check(!wavelengths.snapshot()->preprocessing.initial_peak_usage);
    auto customFinder = std::make_shared<Initial>(); customFinder->selected = {"test.initial", 1};
    AlgorithmSelections selections; selections.initial_peaks = customFinder->identity();
    AlgorithmModules replacement; replacement.initial_peaks = customFinder;
    auto customAlgorithms = resolveAlgorithms(AlgorithmSpec{selections}, replacement); check(bool(customAlgorithms));
    auto customState = gaussian(); customState.algorithms = customAlgorithms.value();
    auto customResult = evaluateInitialPeakSearch(std::make_shared<const ProductState>(customState), configuration, context);
    check(customResult && customResult.value().usage->algorithm == customFinder->identity());
    check(bool(applyInitialPeakSearch(customState, customResult.value())) && bool(validateProductState(customState)));
    std::cout << "PASS: initial-only evaluation, frozen options, private background, invalidation and failures\n";
    return 0;
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
