#include "qualpf/application/preprocessing_evaluation.h"
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
    return status ? evaluation_->preview(request) : status;
}
Status ProductPreprocessingEdit::previewRange(const PreprocessingRequest &request) {
    auto status = admission();
    return status ? evaluation_->previewRange(request) : status;
}
Status ProductPreprocessingEdit::commit() {
    if (closed_)
        return Status{Error{ErrorCode::closed_edit, "preprocessing", "Edit closed"}};
    if (!evaluation_->preview())
        return Status{Error{ErrorCode::missing_data, "preprocessing", "No successful current preview"}};
    stale_ = stale_ || source_ != session_.snapshot() || source_->revision != session_.snapshot()->revision;
    if (stale_)
        return Status{Error{ErrorCode::stale_snapshot, "preprocessing", "Preview source changed"}};
    auto status = interactive_ ? session_.commitPreprocessingEdit(source_, *evaluation_->preview())
                               : session_.commit(source_, *evaluation_->preview());
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
