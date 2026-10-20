#include "qualpf/application/configured_fwhm.h"
#include "qualpf/application/preprocessing_evaluation.h"
#include <cmath>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
static void check(bool value, std::source_location at = std::source_location::current()) {
    if (!value) throw std::runtime_error("Configured FWHM at line " + std::to_string(at.line()));
}
struct Initial final : preprocessing::IInitialPeakFinder {
    mutable int calls = 0;
    AlgorithmIdentity identity() const override { return {"qualpf.peaks.initial.cpp", 5}; }
    Result<preprocessing::InitialPeakResult> find(const AnalysisSpectrum &spectrum, WavelengthAngstrom,
        const preprocessing::InitialPeakOptions &, const ExecutionContext &) const override {
        const auto &p = spectrum.pattern();
        ++calls;
        preprocessing::InitialPeakResult r; r.dataset = p.id; r.candidates = {{10}};
        return Result<preprocessing::InitialPeakResult>{r};
    }
};
struct Gaussian final : preprocessing::IFwhmEstimator {
    mutable int calls = 0;
    bool empty = false;
    mutable std::vector<double> received;
    AlgorithmIdentity identity() const override { return {"qualpf.peaks.fwhm.cpp", 3}; }
    Result<preprocessing::FwhmEstimationResult> estimate(const AnalysisSpectrum &spectrum, const preprocessing::InitialPeakResult &,
        const ExecutionContext &context) const override {
        const auto &p = spectrum.pattern();
        ++calls; received = p.observed;
        check(context.numerical_policy == NumericalPolicy::legacy_mixed_precision);
        preprocessing::FwhmEstimationResult r;
        if (!empty) r.estimate = {preprocessing::FwhmEstimate::State::measured, AngularWidthDegrees{double(.2f)}, 1};
        return Result<preprocessing::FwhmEstimationResult>{r};
    }
};
struct Formal final : preprocessing::IExperimentalPeakExtractor {
    mutable int calls = 0;
    mutable preprocessing::PeakCalibration last;
    AlgorithmIdentity identity() const override { return {"qualpf.peaks.experimental.cpp", 3}; }
    Result<preprocessing::PeakSearchResult> extract(const AnalysisSpectrum &spectrum, const preprocessing::PeakCalibration &c,
        WavelengthAngstrom wave, AngularWidthDegrees, const ExecutionContext &) const override {
        const auto &p = spectrum.pattern();
        ++calls; last = c;
        preprocessing::PeakSearchResult r; r.applied = c.applied;
        r.peaks.dataset = r.candidates.dataset = p.id;
        r.peaks.effective_wavelength = r.candidates.effective_wavelength = wave;
        r.automatic_tolerance = .5;
        return Result<preprocessing::PeakSearchResult>{r};
    }
};
static ProductState triangle() {
    ProductState s; auto &p = s.pattern;
    p.id = {81}; p.source_id = "triangle-with-negative-tails";
    p.radiation.lines = {{WavelengthAngstrom{1.54}, 1}};
    p.processing.background_subtracted = false;
    p.processing.intensity_scaled = false;
    for (int i = 0; i <= 20; ++i) {
        p.two_theta.emplace_back(20 + .01 * i); p.observed.push_back(28 - std::abs(i - 10));
    }
    p.original_observed = p.observed;
    p.background = SampledSignal{std::vector<double>(21, 20), SampleWindow{0}};
    return s;
}
int main() {
    try {
        const ExecutionContext context{{}, NumericalPolicy::legacy_mixed_precision, "configured-fwhm-test"};
        auto s = triangle();
        auto initial = std::make_shared<Initial>(); auto gaussian = std::make_shared<Gaussian>();
        auto formal = std::make_shared<Formal>();
        AlgorithmModules modules; modules.initial_peaks = initial; modules.fwhm = gaussian; modules.experimental_peaks = formal;
        auto algorithms = resolveAlgorithms(AlgorithmSpec{}, modules); check(bool(algorithms)); s.algorithms = algorithms.value();
        ProductSearchSession session(s); const auto source = session.snapshot();
        FwhmConfiguration settings;
        check(settings.default_fwhm.value == .1 && !settings.use_fixed && settings.minimum_sampling_density == 6);
        for (int q : {2, 6, 8}) { settings.minimum_sampling_density = q; check(bool(validateFwhmConfiguration(settings))); }
        settings.minimum_sampling_density = 9; check(!validateFwhmConfiguration(settings)); settings.minimum_sampling_density = 6;
        settings.use_fixed = true;
        auto result = evaluateConfiguredFwhm(source, {source->pattern, settings, {}}, context);
        check(bool(result) && result.value().usage && result.value().usage->origin == FwhmOrigin::fixed);
        check(initial->calls == 0 && gaussian->calls == 0 && session.snapshot() == source);
        auto request = automaticPreprocessingRequest(source, PreprocessingAction::automatic_peaks); check(bool(request));
        request.value().fwhm = settings;
        PreprocessingEvaluation edit(source); check(bool(edit.preview(request.value())));
        check(formal->calls == 1 && initial->calls == 0 && gaussian->calls == 0);
        check(formal->last.source == preprocessing::PeakCalibration::Source::fixed_fwhm);
        check(formal->last.applied.search_range == .9f * float(.1));
        check(edit.preview()->preprocessing.fwhm->default_fwhm.value == .1);
        check(edit.preview()->preprocessing.peak_fwhm_usage->origin == FwhmOrigin::fixed);
        settings.use_fixed = false;
        result = evaluateConfiguredFwhm(source, {source->pattern, settings, {}}, context);
        check(bool(result) && initial->calls == 1 && gaussian->calls == 1);
        check(result.value().usage->origin == FwhmOrigin::gaussian && result.value().usage->width.value == double(.2f));
        check(gaussian->received == source->pattern.observed &&
            result.value().usage->input_pattern->background->values == source->pattern.background->values);
        check(result.value().usage->input_pattern->observed == gaussian->received);
        gaussian->empty = true; request.value().fwhm = settings;
        const auto beforeCalls = formal->calls;
        auto failed = edit.preview(request.value());
        check(!failed && failed.error().reason == "no_eligible_fwhm" && formal->calls == beforeCalls && !edit.preview());
        check(edit.fwhmEvaluation() && !edit.fwhmEvaluation()->usage && session.snapshot() == source);
        gaussian->empty = false;
        settings.method = FwhmMethod::geometric;
        result = evaluateConfiguredFwhm(source, {source->pattern, settings, {}}, context);
        check(bool(result) && result.value().usage && result.value().geometric);
        // Independently: triangular net apex 8 at 20.10, crossings 20.06 and 20.14.
        check(std::abs(result.value().usage->width.value - .08) < 1e-12);
        check(result.value().geometric->noise_model == preprocessing::FwhmNoiseModel::unknown);
        check(result.value().geometric->candidates.front().sampled_height == 8);
        check(initial->calls == 2 && gaussian->calls == 2); // No Gaussian call for geometry.
        auto net = source->pattern;
        for (auto &y : net.observed) y -= 20;
        net.processing.background_subtracted = true; // Old nonzero background retained deliberately.
        auto subtracted = evaluateConfiguredFwhm(source, {net, settings, {}}, context);
        check(!subtracted && subtracted.error().reason == "subtracted_input_refused");
        check(source->pattern.observed == s.pattern.observed && source->pattern.background->values.front() == 20);
        auto historical = *result.value().usage; historical.current = false;
        historical.origin = FwhmOrigin::gaussian; historical.geometric.reset();
        historical.algorithm = {"qualpf.peaks.fwhm.cpp", 2};
        check(bool(validateFwhmUsage(historical))); historical.current = true;
        check(!validateFwhmUsage(historical));
        request.value().fwhm = settings; check(bool(edit.preview(request.value())));
        check(formal->last.source == preprocessing::PeakCalibration::Source::geometric_fwhm);
        check(edit.preview()->preprocessing.peak_fwhm_usage->origin == FwhmOrigin::geometric);
        check(!edit.preview()->preprocessing.fwhm_estimate); // Never forge fitted-width evidence.
        auto changed = *source; check(bool(applyConfiguredFwhm(changed, result.value())));
        changed.preprocessing.extensions["future_preprocessing"] = "retained";
        ProductSearchSession restartSession(changed);
        PreprocessingEvaluation restart(restartSession.snapshot());
        PreprocessingRequest restartRequest; restartRequest.restart_from_original = true;
        check(bool(restart.preview(restartRequest)));
        check(restart.preview()->preprocessing.fwhm == changed.preprocessing.fwhm);
        check(!restart.preview()->preprocessing.fwhm_usage->current);
        check(restart.preview()->preprocessing.extensions == changed.preprocessing.extensions);
        invalidateAutomaticFwhm(changed.preprocessing);
        check(!changed.preprocessing.fwhm_usage->current && changed.preprocessing.fwhm->default_fwhm.value > 0);
        auto partial = source->pattern; partial.background->window.reset();
        check(!evaluateConfiguredFwhm(source, {partial, settings, {}}, context));
        auto missing = source->pattern; missing.background.reset();
        auto preparedBackground = evaluateConfiguredFwhm(source, {missing, settings, {}}, context);
        check(bool(preparedBackground) && source->pattern.background->values.front() == 20);
        auto flat = source->pattern; flat.observed.assign(21, 20);
        result = evaluateConfiguredFwhm(source, {flat, settings, {}}, context);
        check(bool(result) && !result.value().usage && result.value().geometric);
        check(!applyConfiguredFwhm(changed, result.value()));
        std::stop_source stop; stop.request_stop(); auto canceled = context; canceled.cancellation = stop.get_token();
        auto error = evaluateConfiguredFwhm(source, {source->pattern, settings, {}}, canceled);
        check(!error && error.error().code == ErrorCode::cancelled && session.snapshot() == source);
        std::cout << "Configured FWHM: physical widths, explicit signals, skip/stop policy and source binding passed\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
