#include "qualpf/application/preprocessing_evaluation.h"
#include <algorithm>
#include <cmath>
namespace qualpf::application {
Result<PreprocessingRequest> automaticPreprocessingRequest(ProductHandle source, PreprocessingAction action) {
    if (!source || source->editing)
        return Result<PreprocessingRequest>{
            Error{ErrorCode::stale_snapshot, "preprocessing", "Finish the open edit first"}};
    if (source->pattern.two_theta.empty() || source->pattern.radiation.lines.empty())
        return Result<PreprocessingRequest>{
            Error{ErrorCode::missing_data, "preprocessing", "Pattern and wavelength are required"}};
    if (!source->pattern.processing.background_subtracted)
        return Result<PreprocessingRequest>{Error{
            ErrorCode::missing_data, "background.state",
            "Subtraction history is unknown; choose an explicit new input", "subtraction_state_unknown"}};
    PreprocessingRequest request;
    request.wavelength = source->pattern.radiation.lines.front().wavelength;
    request.tolerance = AngularWidthDegrees{source->peakTolerance.value_or(.15)};
    const bool subtract = action != PreprocessingAction::automatic_peaks;
    request.peak_product_operation = action == PreprocessingAction::automatic_peaks;
    if (!source->pattern.background) {
        if (*source->pattern.processing.background_subtracted)
            return Result<PreprocessingRequest>{Error{
                ErrorCode::missing_data, "background.state",
                "Subtracted input has no aligned background representation", "background_alignment_unknown"}};
        request.background = source->preprocessing.background.value_or(preprocessing::BackgroundOptions{});
    }
    request.subtract_background = subtract;
    if (action != PreprocessingAction::subtract_background) {
        request.peaks = source->preprocessing.peaks.value_or(preprocessing::PeakOptions{});
        request.peaks->search_range = -1;
        request.peaks->detection_range.reset();
        request.use_background_for_peaks = true;
    }
    return Result<PreprocessingRequest>{std::move(request)};
}
Result<PreprocessingRequest> backgroundPointRequest(ProductHandle source, bool add, double x, double y,
                                                    bool redetect) {
    if (!source || source->editing || source->pattern.two_theta.empty() ||
        source->pattern.radiation.lines.empty())
        return Result<PreprocessingRequest>{
            Error{ErrorCode::missing_data, "background.points", "A closed, loaded pattern is required"}};
    if (!std::isfinite(float(x)) || !std::isfinite(float(y)))
        return Result<PreprocessingRequest>{
            Error{ErrorCode::invalid_input, "background.points", "Finite point coordinates are required"}};
    PreprocessingRequest request;
    request.wavelength = source->pattern.radiation.lines.front().wavelength;
    request.tolerance = AngularWidthDegrees{source->peakTolerance.value_or(.15)};
    request.background = source->preprocessing.background.value_or(preprocessing::BackgroundOptions{});
    if (request.background->method == preprocessing::BackgroundMethod::spline ||
        request.background->method == preprocessing::BackgroundMethod::bezier ||
        request.background->method == preprocessing::BackgroundMethod::bruckner)
        return Result<PreprocessingRequest>{Error{ErrorCode::invalid_input, "background.points",
                                                  "This method does not consume manual background points",
                                                  "manual_points_not_consumed_by_method"}};
    if (source->preprocessing.background_points)
        request.background_points = source->preprocessing.background_points;
    else {
        auto initial = preprocessing::background(source->pattern, *request.background, request.wavelength);
        if (!initial)
            return Result<PreprocessingRequest>{initial.error()};
        request.background_points = std::move(initial).value().points;
    }
    auto &points = *request.background_points;
    if (add) {
        auto at =
            std::lower_bound(points.begin(), points.end(), float(x), [](auto p, float v) { return p.x < v; });
        points.insert(at, {float(x), float(y)});
    } else {
        if (points.size() <= 2)
            return Result<PreprocessingRequest>{Error{ErrorCode::invalid_input, "background.points",
                                                      "At least two background points must remain"}};
        auto at = std::min_element(points.begin(), points.end(), [&](auto a, auto b) {
            return std::abs(a.x - float(x)) < std::abs(b.x - float(x));
        });
        points.erase(at);
    }
    if (redetect) {
        request.peaks = source->preprocessing.peaks.value_or(preprocessing::PeakOptions{});
        request.peaks->search_range = -1;
        request.peaks->detection_range.reset();
        request.use_background_for_peaks = true;
    }
    return Result<PreprocessingRequest>{std::move(request)};
}
} // namespace qualpf::application
