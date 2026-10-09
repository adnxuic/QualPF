#include "classic_fixture.h"
#include "qualpf/application/preprocessing_evaluation.h"
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <thread>
using namespace qualpf;
using namespace qualpf::application;
static void check(bool v, std::source_location at = std::source_location::current()) {
    if (!v)
        throw std::runtime_error("Preprocessing contract failed at line " + std::to_string(at.line()));
}
static Result<std::vector<double>> unchanged(const Pattern &p) {
    return Result<std::vector<double>>{p.observed};
}
static std::stop_source *stopAtCalculation=nullptr;
static Result<std::vector<double>> cancelling(const Pattern &p) {
    stopAtCalculation->request_stop();return Result<std::vector<double>>{p.observed};
}
static Result<std::vector<double>> failed(const Pattern &) {
    return Result<std::vector<double>>{Error{ErrorCode::numerical_failure, "test", "controlled failure"}};
}
static void kalpha2Selection() {
    check(selectKalpha2Backend("", "") == Kalpha2Backend::cpp);
    check(selectKalpha2Backend("", "legacy") == Kalpha2Backend::legacy);
    check(selectKalpha2Backend("", "invalid") == Kalpha2Backend::cpp);
    check(selectKalpha2Backend("cpp", "legacy") == Kalpha2Backend::cpp);
    check(selectKalpha2Backend("legacy", "cpp") == Kalpha2Backend::legacy);
    for (const auto *invalid : {"invalid", "CPP", "LEGACY", " cpp"})
        check(selectKalpha2Backend(invalid, "legacy") == Kalpha2Backend::invalid);
    struct Environment final : EnvironmentReader {
        std::string dedicated = "invalid", preprocessing = "cpp";
        mutable int reads = 0;
        std::string environmentValue(const std::string &name) const override {
            ++reads;
            return name == "QUALPF_KALPHA2_BACKEND" ? dedicated : preprocessing;
        }
    } environment;
    ProductState state;
    state.pattern.id = {1};
    state.pattern.processing.kalpha2_subtracted = false;
    state.pattern.radiation = {RadiationKind::x_ray, {{WavelengthAngstrom{1.54059307}, 1}}};
    for (int i = 0; i < 1000; ++i) {
        state.pattern.two_theta.emplace_back(10 + i * .02);
        state.pattern.observed.push_back(1);
    }
    ProductSearchSession session(state);
    const auto source = session.snapshot();
    auto invalid = stripKalpha2Operation(session, environment);
    check(!invalid && invalid.error().reason == "invalid_kalpha2_backend" && session.snapshot() == source && environment.reads == 2);
    environment.dedicated = "cpp";
    environment.preprocessing = "legacy";
    const auto expected = stripKalpha2Cpp(state.pattern);
    check(expected && stripKalpha2Operation(session, environment) && environment.reads == 4 &&
          session.snapshot()->pattern.observed == expected.value());
    environment.dedicated = "legacy";
    const auto stripped = session.snapshot();
    check(!stripKalpha2Operation(session, environment) && environment.reads == 6 && session.snapshot() == stripped);
    state.pattern.observed[0] = -3e38;
    state.pattern.observed[1] = 3e38;
    const auto failure = stripKalpha2Compatible(state.pattern);
    check(!failure && failure.error().code == ErrorCode::numerical_failure &&
          failure.error().field == "pattern" && failure.error().reason == "kalpha2_nonfinite_result" &&
          failure.error().message == "Native stripping produced a nonfinite result");
    ProductSearchSession overflow(state);
    const auto unchangedSource = overflow.snapshot();
    check(!stripKalpha2Operation(overflow, environment) && overflow.snapshot() == unchangedSource);
}
int main() {
    kalpha2Selection();
    testing::ClassicFixture f;
    ProductState state;
    state.pattern = f.pattern;
    state.pattern.original_observed = state.pattern.observed;
    state.pattern.radiation.kind = RadiationKind::x_ray;
    state.pattern.radiation.lines = {{WavelengthAngstrom{1.5406}, 1}};
    state.pattern.processing.kalpha2_subtracted = false;
    state.pattern.processing.background_subtracted = false;
    state.peaks.valid = true;
    state.peaks.wave = 1.5406;
    state.peaks.integratedIntensity.emplace();
    for (const auto &p : f.peaks.peaks) {
        state.peaks.tth.push_back(p.two_theta.value);
        state.peaks.d.push_back(p.d_spacing->value);
        state.peaks.deltaD.push_back(p.delta_d->value);
        state.peaks.intensity.push_back(p.intensity);
        state.peaks.intensityOrig.push_back(*p.original_intensity);
        state.peaks.fwhm.push_back(p.fwhm->value);
        state.peaks.integratedIntensity->push_back(*p.integrated_intensity);
    }
    ProductCard sentinel;
    sentinel.id = "retained";
    state.candidates = {sentinel};
    state.phases = {sentinel};
    ProductSearchSession session(state);
    auto source = session.snapshot();
    PreprocessingRequest range;
    range.crop_range = AngularRange{TwoThetaDegrees{0}, TwoThetaDegrees{60}};
    range.wavelength = WavelengthAngstrom{1.5406};
    range.tolerance = AngularWidthDegrees{.15};
    PreprocessingEvaluation evaluation(source);
    check(bool(evaluation.previewRange(range)) && evaluation.preview()->candidates == state.candidates);
    check(session.snapshot() == source &&
          rangeNumericalEquivalent(evaluation.preview()->pattern, state.pattern));
    range.crop_range = AngularRange{TwoThetaDegrees{10}, TwoThetaDegrees{40}};
    check(bool(evaluation.previewRange(range)) && evaluation.preview()->pattern.observed.size() == 4);
    check(evaluation.preview()->candidates.empty() && evaluation.preview()->phases.empty() &&
          evaluation.preview()->peaks.tth.empty());
    range.crop_range = AngularRange{TwoThetaDegrees{17}, TwoThetaDegrees{18}};
    check(!evaluation.previewRange(range) && !evaluation.preview() && session.snapshot() == source);
    PreprocessingRequest strip;
    strip.strip_kalpha2 = true;
    PreprocessingEvaluation noOp(source, unchanged);
    check(bool(noOp.preview(strip)) && noOp.preview()->pattern.processing.kalpha2_subtracted == true);
    check(noOp.preview()->peaks == state.peaks && noOp.preview()->candidates == state.candidates);
    PreprocessingEvaluation failure(source, failed);
    check(!failure.preview(strip) && !failure.preview());
    std::stop_source cancellation;
    cancellation.request_stop();
    check(!noOp.preview(strip, cancellation.get_token()) && !noOp.preview());
    std::stop_source during;
    stopAtCalculation=&during;
    PreprocessingEvaluation duringCalculation(source,cancelling);
    auto aborted=duringCalculation.preview(strip,during.get_token());
    check(!aborted && aborted.error().code==ErrorCode::cancelled && !duringCalculation.preview() && session.snapshot()==source);
    ProductPreprocessingEdit edit(session, true, unchanged);
    check(session.snapshot()->editing);
    check(bool(edit.preview(strip)));
    edit.cancel();
    check(!session.snapshot()->editing);
    check(session.snapshot()->pattern.processing.kalpha2_subtracted == false);
    ProductPreprocessingEdit stale(session, false, unchanged);
    check(bool(stale.preview(strip)));
    session.replacePeaks(state.peaks);
    check(!stale.commit());
    const auto latest = session.snapshot();
    bool workerOk = false;
    std::thread worker([&] {
        PreprocessingEvaluation privateWork(latest, unchanged);
        workerOk = bool(privateWork.preview(strip)) && privateWork.preview()->candidates == state.candidates;
    });
    worker.join();
    check(workerOk && session.snapshot() == latest);
    ProductPreprocessingEdit apply(session, false, unchanged);
    check(bool(apply.preview(strip)) && bool(apply.commit()));
    check(session.snapshot()->pattern.processing.kalpha2_subtracted == true &&
          session.snapshot()->peaks == state.peaks);
    std::cout << "PASS: pure worker evaluation, Range no-op/invalidation, Kalpha2 provenance-only no-op, "
                 "failure/cancel, editor cancel/stale and atomic apply\n";
}
