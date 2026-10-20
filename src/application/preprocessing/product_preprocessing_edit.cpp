#include "qualpf/application/preprocessing_evaluation.h"
#include <algorithm>
#include <stdexcept>

namespace qualpf::application {
ProductPreprocessingEdit::ProductPreprocessingEdit(ProductSearchSession &session, bool interactive,
                                                   Stripper stripper)
    : session_(session), interactive_(interactive) {
    if (interactive_ && session_.snapshot()->editing)
        throw std::logic_error("Cannot nest C++ preprocessing editors");
    if (interactive_)
        session_.beginPreprocessingEdit();
    source_ = session_.snapshot();
    try {
        evaluation_ = std::make_unique<PreprocessingEvaluation>(source_, stripper);
    } catch (...) {
        if (interactive_)
            session_.cancelPreprocessingEdit(source_);
        throw;
    }
}
ProductPreprocessingEdit::~ProductPreprocessingEdit() { cancel(); }
Status ProductPreprocessingEdit::admission() {
    if (closed_) {
        evaluation_->clear();
        return Status{Error{ErrorCode::closed_edit, "preprocessing", "Edit closed"}};
    }
    stale_ = stale_ || source_ != session_.snapshot() || source_->revision != session_.snapshot()->revision;
    if (stale_ || (source_->editing && !interactive_)) {
        evaluation_->clear();
        return Status{
            Error{ErrorCode::stale_snapshot, "preprocessing", "Source changed or another editor is open"}};
    }
    return Status{std::monostate{}};
}
Status ProductPreprocessingEdit::preview(const PreprocessingRequest &request) {
    auto status = admission();
    if (status) status = evaluation_->preview(request);
    lastRequest_ = status ? std::optional<PreprocessingRequest>{request} : std::nullopt;
    return status;
}
Status ProductPreprocessingEdit::previewRange(const PreprocessingRequest &request) {
    auto status = admission();
    if (status) status = evaluation_->previewRange(request);
    lastRequest_ = status ? std::optional<PreprocessingRequest>{request} : std::nullopt;
    return status;
}
Status ProductPreprocessingEdit::commit() {
    if (closed_)
        return Status{Error{ErrorCode::closed_edit, "preprocessing", "Edit closed"}};
    if (!evaluation_->preview())
        return Status{Error{ErrorCode::missing_data, "preprocessing", "No successful current preview"}};
    stale_ = stale_ || source_ != session_.snapshot() || source_->revision != session_.snapshot()->revision;
    if (stale_)
        return Status{Error{ErrorCode::stale_snapshot, "preprocessing", "Preview source changed"}};
    auto next = *evaluation_->preview();
    if (interactive_ && next.projectSettings && lastRequest_) {
        const auto &r = *lastRequest_;
        auto &profile = next.projectSettings->profile(r.workflow.value_or(WorkflowId::preprocessing));
        if (r.background) profile.background = *r.background;
        if (r.smoothing) profile.smoothing = *r.smoothing;
        if (r.crop_range) {
            // The native editor accepts apertures beyond the data/domain. Keep
            // that behavior; saved parameters use the physical angular domain,
            // rather than the sample-aligned window produced by cropping.
            profile.range = AngularRange{TwoThetaDegrees{std::max(0., r.crop_range->minimum.value)},
                                         TwoThetaDegrees{std::min(180., r.crop_range->maximum.value)}};
        }
        // A frozen pool selection normalizes count/append/threshold controls.
        // Persist the applied controls so reopening does not resurrect old inputs.
        const bool reselected = r.peak_product_operation && r.peak_operation == PeakOperation::reselect;
        if (r.peaks) profile.peaks = reselected && next.preprocessing.peaks ? *next.preprocessing.peaks : *r.peaks;
        if (r.selection) profile.selection = reselected && next.preprocessing.selection ? *next.preprocessing.selection : *r.selection;
        if (r.fwhm) profile.fwhm = *r.fwhm;
        if (r.initial_peaks) profile.initial_peaks = *r.initial_peaks;
        auto valid = validateProjectSettings(*next.projectSettings);
        if (!valid) return valid;
    }
    auto status = interactive_ ? session_.commitPreprocessingEdit(source_, std::move(next))
                               : session_.commit(source_, std::move(next));
    if (status) {
        closed_ = true;
        evaluation_->clear();
    }
    return status;
}
void ProductPreprocessingEdit::cancel() {
    if (closed_)
        return;
    evaluation_->clear();
    closed_ = true;
    if (interactive_)
        session_.cancelPreprocessingEdit(source_);
}
} // namespace qualpf::application
