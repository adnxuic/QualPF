#include "qualpf/application/preprocessing_evaluation.h"
#include "qualpf/application/workflow.h"
#include <algorithm>
#include <cmath>
namespace qualpf::application {
Result<PreprocessingRequest> automaticPreprocessingRequest(ProductHandle source, PreprocessingAction action, SearchMethod method) {
    if (action == PreprocessingAction::prepare_search) {
        auto available = preflightSearchMethod(method);
        if (!available) return Result<PreprocessingRequest>{available.error()};
        if (method == SearchMethod::pfsm) return Result<PreprocessingRequest>{Error{ErrorCode::unsupported,
            "pfsm.preparation","PFSM requires an explicit observation request, not formal peak preparation","pfsm_requires_observations"}};
    }
    if (!source || source->editing)
        return Result<PreprocessingRequest>{
            Error{ErrorCode::stale_snapshot, "preprocessing", "Finish the open edit first"}};
    if (source->pattern.two_theta.empty() || source->pattern.radiation.lines.empty())
        return Result<PreprocessingRequest>{
            Error{ErrorCode::missing_data, "preprocessing", "Pattern and wavelength are required"}};
    if (source->pattern.processing.background_subtracted != false)
        return Result<PreprocessingRequest>{Error{
            ErrorCode::missing_data, "background.state",
            "Restart preprocessing or choose an explicitly unsubtracted input",
            source->pattern.processing.background_subtracted ? "subtracted_input_refused" : "subtraction_state_unknown"}};
    PreprocessingRequest request;
    const auto flow = action == PreprocessingAction::prepare_search ? WorkflowId::csm :
        action == PreprocessingAction::automatic_peaks ? WorkflowId::peak_search : WorkflowId::preprocessing;
    request.workflow = flow;
    const auto *profile = source->projectSettings ? &source->projectSettings->profile(flow) : nullptr;
    request.algorithms = source->algorithms;
    request.wavelength = source->pattern.radiation.lines.front().wavelength;
    request.tolerance = AngularWidthDegrees{source->peakTolerance.value_or(.15)};
    request.peak_product_operation = action == PreprocessingAction::automatic_peaks;
    if (!source->pattern.background || (profile && !reusableWorkflowStep(*source,flow,WorkflowStep::background))) {
        request.background = profile ? profile->background : source->preprocessing.background.value_or(source->algorithms->spec.defaults().background);
    }
    if (action == PreprocessingAction::generate_background) {
        request.background = profile ? profile->background : source->preprocessing.background.value_or(source->algorithms->spec.defaults().background);
        if (request.background->method == preprocessing::BackgroundMethod::bruckner)
            request.background->bruckner_range = AngularRange{source->pattern.two_theta.front(), source->pattern.two_theta.back()};
    } else {
        request.peaks = profile ? profile->peaks : source->preprocessing.peaks.value_or(source->algorithms->spec.defaults().peaks);
        if (profile) {
            request.initial_peaks = profile->initial_peaks;
            request.fwhm = profile->fwhm;
            request.selection = profile->selection;
            request.peaks->maximum_peaks = profile->selection.maximum_peaks;
            request.peaks->threshold_percent = profile->selection.threshold_percent;
            if (source->preprocessing.fwhm_usage &&
                (!request.background || source->preprocessing.fwhm_usage->origin == FwhmOrigin::fixed) &&
                reusableWorkflowStep(*source,flow,WorkflowStep::fwhm)) {
                request.supplied_width = source->preprocessing.fwhm_usage;
                request.fwhm = source->preprocessing.fwhm;
            }
        }
        request.peaks->search_range = -1;
        request.peaks->detection_range.reset();
        request.purpose = action == PreprocessingAction::prepare_search ? AnalysisPurpose::classical_search : AnalysisPurpose::peak_measurement;
    }
    return Result<PreprocessingRequest>{std::move(request)};
}
Result<PreprocessingRequest> backgroundPointRequest(ProductHandle source, bool add, double x, double y,
                                                    bool redetect) {
    if (!source || source->editing || source->pattern.two_theta.empty() ||
        source->pattern.radiation.lines.empty())
        return Result<PreprocessingRequest>{
            Error{ErrorCode::missing_data, "background.points", "A closed, loaded pattern is required"}};
    if (source->pattern.processing.background_subtracted != false)
        return Result<PreprocessingRequest>{Error{ErrorCode::missing_data,"background.state",
            "Background editing requires an explicitly unsubtracted signal",
            source->pattern.processing.background_subtracted ? "subtracted_input_refused" : "subtraction_state_unknown"}};
    if (!std::isfinite(float(x)) || !std::isfinite(float(y)))
        return Result<PreprocessingRequest>{
            Error{ErrorCode::invalid_input, "background.points", "Finite point coordinates are required"}};
    PreprocessingRequest request;
    request.algorithms = source->algorithms;
    request.wavelength = source->pattern.radiation.lines.front().wavelength;
    request.tolerance = AngularWidthDegrees{source->peakTolerance.value_or(.15)};
    request.background = source->preprocessing.background.value_or(source->algorithms->spec.defaults().background);
    if (request.background->method == preprocessing::BackgroundMethod::spline ||
        request.background->method == preprocessing::BackgroundMethod::bezier ||
        request.background->method == preprocessing::BackgroundMethod::bruckner)
        return Result<PreprocessingRequest>{Error{ErrorCode::invalid_input, "background.points",
                                                  "This method does not consume manual background points",
                                                  "manual_points_not_consumed_by_method"}};
    if (source->preprocessing.background_points)
        request.background_points = source->preprocessing.background_points;
    else {
        auto initial = source->algorithms->modules.background->estimate(source->pattern, *request.background,
            request.wavelength, std::nullopt, {{}, NumericalPolicy::legacy_mixed_precision, "product-preprocessing"});
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
        request.peaks = source->preprocessing.peaks.value_or(source->algorithms->spec.defaults().peaks);
        request.peaks->search_range = -1;
        request.peaks->detection_range.reset();
    }
    return Result<PreprocessingRequest>{std::move(request)};
}
} // namespace qualpf::application
