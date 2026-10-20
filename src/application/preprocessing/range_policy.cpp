#include "qualpf/application/preprocessing_evaluation.h"
#include "qualpf/application/configured_fwhm.h"

namespace qualpf::application {
namespace {
bool sameSignal(const std::optional<SampledSignal> &a, const std::optional<SampledSignal> &b) {
    return (!a && !b) ||
           (a && b && a->values == b->values && a->window.has_value() == b->window.has_value() &&
            (!a->window || a->window->first == b->window->first));
}
} // namespace

bool rangeNumericalEquivalent(const Pattern &a, const Pattern &b) {
    if (a.id != b.id || a.two_theta != b.two_theta || a.observed != b.observed ||
        a.original_observed != b.original_observed || a.corrected_two_theta != b.corrected_two_theta ||
        a.weights != b.weights || !sameSignal(a.background, b.background) ||
        !sameSignal(a.calculated, b.calculated) ||
        !sameSignal(a.background_subtracted_signal, b.background_subtracted_signal) ||
        a.calculation_range.has_value() != b.calculation_range.has_value() ||
        a.processing.background_subtracted != b.processing.background_subtracted ||
        a.processing.kalpha2_subtracted != b.processing.kalpha2_subtracted ||
        a.processing.intensity_scaled != b.processing.intensity_scaled ||
        a.intensity_scale != b.intensity_scale ||
        a.original_kalpha2_subtracted != b.original_kalpha2_subtracted ||
        a.radiation.kind != b.radiation.kind || a.radiation.lines.size() != b.radiation.lines.size())
        return false;
    if (a.calculation_range && (a.calculation_range->minimum != b.calculation_range->minimum ||
                                a.calculation_range->maximum != b.calculation_range->maximum))
        return false;
    for (std::size_t i = 0; i < a.radiation.lines.size(); ++i)
        if (a.radiation.lines[i].wavelength != b.radiation.lines[i].wavelength ||
            a.radiation.lines[i].relative_weight != b.radiation.lines[i].relative_weight)
            return false;
    return true;
}

void finalizeRangeState(ProductState &next, const ProductState &opening) {
    if (rangeNumericalEquivalent(next.pattern, opening.pattern)) {
        next = opening;
        return;
    }
    // Method/options remain configuration; points/filter history belonged to
    // the previous domain. RP1 alone owns all sampled values and windows.
    next.preprocessing.background_points.reset();
    next.preprocessing.background_filters.clear();
    next.peaks.clear();
    next.preprocessing.peak_candidates.reset();
    next.preprocessing.peak_calibration.reset();
    next.preprocessing.fwhm_estimate.reset();
    next.preprocessing.peak_fwhm_usage.reset();
    invalidateAutomaticFwhm(next.preprocessing);
    next.preprocessing.rejected_candidates.clear();
    next.preprocessing.candidate_failure.reset();
    next.preprocessing.selection.reset();
    next.peakTolerance.reset();
    next.automaticDelta = .5; // Existing empty-set convention, not a measured estimate.
    next.candidates.clear();
    next.phases.clear();
    next.projectMetadata["has_back"] = hasActiveBackground(next.pattern);
    next.projectMetadata["candidate_pool_state"] = "invalidated_by_structural_crop";
    next.projectMetadata["peak_snapshot_origin"] = "unavailable_after_structural_crop";
    next.projectMetadata["residual_current_known"] = true;
}

Status PreprocessingEvaluation::previewRange(const PreprocessingRequest &request, std::stop_token token) {
    // Admission precedes the unchanged constructor's slicing. No malformed
    // dimensions can reach iterator arithmetic, even for an injected source.
    draft_.reset();
    if (token.stop_requested())
        return Status{Error{ErrorCode::cancelled, "preprocessing", "Preprocessing canceled"}};
    if (!source_)
        return Status{Error{ErrorCode::missing_data, "preprocessing.source", "Missing source"}};
    if (!request.crop_range)
        return Status{Error{ErrorCode::invalid_input, "pattern.range", "Expected a crop request"}};
    auto valid = validateProductState(*source_);
    if (!valid)
        return valid;
    auto status = preview(request, token); // Existing selection and RP1 source remain unchanged.
    if (!status)
        return status;
    auto next = *draft_;
    finalizeRangeState(next, *source_);
    valid = validateProductState(next);
    if (!valid) {
        draft_.reset();
        return valid;
    }
    candidateCount_ =
        next.preprocessing.peak_candidates ? next.preprocessing.peak_candidates->peaks.size() : 0;
    draft_ = std::move(next);
    return Status{std::monostate{}};
}
} // namespace qualpf::application
