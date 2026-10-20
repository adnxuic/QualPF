#include "qualpf/application/preprocessing_workflows.h"
#include "qualpf/application/search_engine.h"
#include "qualpf/domain/validation.h"
#include <algorithm>
#include <cmath>

namespace qualpf::application {
namespace {
Error cancelled() { return {ErrorCode::cancelled, "preprocessing", "Preprocessing canceled"}; }
Error badOutput(std::string field, std::string message) {
    return {ErrorCode::invalid_input, std::move(field), std::move(message), "invalid_algorithm_output"};
}
Status boundInput(const ProductHandle &source, const Pattern &pattern) {
    if (!source || !source->algorithms)
        return Status{Error{ErrorCode::missing_data, "preprocessing.source", "Missing source"}};
    if (pattern.id != source->pattern.id || pattern.source_id != source->pattern.source_id)
        return Status{Error{ErrorCode::stale_snapshot, "preprocessing.source", "Input belongs to another pattern"}};
    if (pattern.processing.background_subtracted != false)
        return Status{Error{ErrorCode::missing_data, "background.state",
            "Restart preprocessing or reimport an explicitly unsubtracted signal",
            pattern.processing.background_subtracted ? "subtracted_input_refused" : "subtraction_state_unknown"}};
    const auto issues = validate(pattern);
    if (!issues.empty()) return Status{issues.front()};
    if (pattern.observed.empty() || pattern.observed.size() != pattern.two_theta.size())
        return Status{Error{ErrorCode::missing_data, "preprocessing.pattern", "A continuous pattern is required"}};
    return Status{std::monostate{}};
}
} // namespace
Result<CommonPreprocessingResult> evaluateCommonPreprocessing(ProductHandle source,
    const CommonPreprocessingRequest &request, const ExecutionContext &context) {
    if (!source) return Result<CommonPreprocessingResult>{Error{ErrorCode::missing_data, "preprocessing.source", "Missing source"}};
    return evaluateCommonPreprocessing(source, source->pattern, request, context);
}
Result<CommonPreprocessingResult> evaluateCommonPreprocessing(ProductHandle source, Pattern pattern,
    const CommonPreprocessingRequest &request, const ExecutionContext &context) {
    if (context.cancellation.stop_requested()) return Result<CommonPreprocessingResult>{cancelled()};
    auto valid = boundInput(source, pattern);
    if (!valid) return Result<CommonPreprocessingResult>{valid.error()};
    CommonPreprocessingResult result;
    result.source = source;
    if (request.background_points && !request.background)
        return Result<CommonPreprocessingResult>{Error{ErrorCode::invalid_input, "background.points",
            "Manual points require explicit background parameters"}};
    if (request.background) {
        if (request.background_points &&
            (request.background->method == preprocessing::BackgroundMethod::spline ||
             request.background->method == preprocessing::BackgroundMethod::bezier ||
             request.background->method == preprocessing::BackgroundMethod::bruckner))
            return Result<CommonPreprocessingResult>{Error{ErrorCode::invalid_input, "background.points",
                "This method does not consume manual background points", "manual_points_not_consumed_by_method"}};
        const auto points = request.background_points ?
            std::optional<std::span<const preprocessing::BackgroundPoint>>{*request.background_points} : std::nullopt;
        auto estimated = source->algorithms->modules.background->estimate(pattern, *request.background,
            request.wavelength, points, context);
        if (!estimated) return Result<CommonPreprocessingResult>{estimated.error()};
        result.background = std::move(estimated).value();
        if (result.background->values.size() != pattern.observed.size() ||
            std::any_of(result.background->values.begin(), result.background->values.end(),
                        [](double y) { return !std::isfinite(y); }))
            return Result<CommonPreprocessingResult>{badOutput("algorithm.output.background", "Invalid background samples")};
        pattern.background = SampledSignal{result.background->values, SampleWindow{0}};
    }
    for (const auto &filter : request.background_filters) {
        auto filtered = preprocessing::filter_background(pattern, filter);
        if (!filtered) return Result<CommonPreprocessingResult>{filtered.error()};
        pattern.background = std::move(filtered).value();
    }
    if (request.smoothing) {
        if (request.smoothing->derivative != 0)
            return Result<CommonPreprocessingResult>{Error{ErrorCode::invalid_input, "preprocessing.derivative",
                "Derivative preview cannot replace the working intensity"}};
        auto smoothed = preprocessing::smooth(pattern.observed, *request.smoothing);
        if (!smoothed) return Result<CommonPreprocessingResult>{smoothed.error()};
        pattern.observed = std::move(smoothed).value();
    }
    result.working_pattern = std::move(pattern);
    if (request.peaks || (result.working_pattern.background && result.working_pattern.background->window &&
        result.working_pattern.background->window->first == 0 &&
        result.working_pattern.background->values.size() == result.working_pattern.observed.size())) {
        auto spectrum = AnalysisSpectrum::create(result.working_pattern, request.purpose);
        if (!spectrum) return Result<CommonPreprocessingResult>{spectrum.error()};
        result.spectrum = std::make_shared<const AnalysisSpectrum>(std::move(spectrum).value());
    }
    if (request.estimate_fwhm && request.peaks && request.peaks->search_range < 0) {
        auto initial = source->algorithms->modules.initial_peaks->find(*result.spectrum,
            request.wavelength, request.initial_peaks.value_or(
                source->preprocessing.initial_peaks.value_or(InitialPeakConfiguration{}).options), context);
        if (!initial) return Result<CommonPreprocessingResult>{initial.error()};
        valid = preprocessing::validate_initial_peak_candidates(initial.value(), result.spectrum->pattern());
        if (!valid) return Result<CommonPreprocessingResult>{valid.error()};
        result.initial_peaks = std::move(initial).value();
        auto width = source->algorithms->modules.fwhm->estimate(*result.spectrum, *result.initial_peaks, context);
        if (!width) return Result<CommonPreprocessingResult>{width.error()};
        valid = preprocessing::validate_fwhm_result(width.value(), *result.initial_peaks, result.spectrum->pattern());
        if (!valid) return Result<CommonPreprocessingResult>{valid.error()};
        result.fwhm = std::move(width).value();
    }
    if (context.cancellation.stop_requested()) return Result<CommonPreprocessingResult>{cancelled()};
    return Result<CommonPreprocessingResult>{std::move(result)};
}
Result<preprocessing::PeakSearchResult> evaluateFormalPeakExtraction(ProductHandle source,
    const FormalPeakExtractionRequest &request, const ExecutionContext &context) {
    using Return = Result<preprocessing::PeakSearchResult>;
    if (context.cancellation.stop_requested()) return Return{cancelled()};
    if (!request.spectrum) return Return{Error{ErrorCode::missing_data, "peaks.spectrum", "A checked spectrum is required"}};
    const auto &pattern = request.spectrum->pattern();
    auto valid = boundInput(source, pattern);
    if (!valid) return Return{valid.error()};
    const auto &parameters = request.parameters;
    if (!std::isfinite(parameters.formal_search_range.value) || parameters.formal_search_range.value < 0 ||
        parameters.formal_search_range.value != double(parameters.applied.search_range))
        return Return{Error{ErrorCode::invalid_input, "peaks.calibration", "Inconsistent calibrated search range"}};
    auto extracted = source->algorithms->modules.experimental_peaks->extract(*request.spectrum, parameters,
        request.wavelength, request.tolerance, context);
    if (!extracted) return extracted;
    auto &r = extracted.value();
    valid = validatePeakRejections(r.rejected_candidates, pattern,
                                   preprocessing::PeakDetectionStage::formal);
    if (!valid) return Return{valid.error()};
    if (r.peaks.dataset != pattern.id || r.candidates.dataset != pattern.id)
        return Return{badOutput("algorithm.output.dataset", "Extractor peaks and candidates must belong to the input pattern")};
    auto measured = productMeasuredPeaks(r.peaks);
    if (!measured) return Return{measured.error()};
    auto pool = r.candidates.effective_wavelength ? productMeasuredPeaks(r.candidates) :
        productMeasuredPeaks(r.candidates, false, request.wavelength);
    if (!pool) return Return{pool.error()};
    if (!std::isfinite(r.automatic_tolerance) || r.automatic_tolerance < 0)
        return Return{badOutput("algorithm.output.tolerance", "Extractor returned an invalid automatic tolerance")};
    r.calibration = parameters;
    if (context.cancellation.stop_requested()) return Return{cancelled()};
    return extracted;
}
} // namespace qualpf::application
