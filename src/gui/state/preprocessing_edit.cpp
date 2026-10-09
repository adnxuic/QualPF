#include "gui/state/preprocessing_edit.h"
namespace qualpf::gui {
using namespace application;
PreprocessingEdit::PreprocessingEdit(SessionView &session, bool interactive, Stripper stripper)
    : edit_(session.canonicalSession(), interactive, stripper) {}
void PreprocessingEdit::refresh() {
    view_.reset();
    if (const auto *draft = edit_.preview())
        view_ = searchView(std::make_shared<const ProductState>(*draft));
}
Status PreprocessingEdit::preview(const PreprocessingRequest &request) {
    auto status = edit_.preview(request);
    refresh();
    return status;
}
Status PreprocessingEdit::previewRange(const PreprocessingRequest &request) {
    auto status = edit_.previewRange(request);
    refresh();
    return status;
}
Status PreprocessingEdit::commit() {
    auto status = edit_.commit();
    refresh();
    return status;
}
void PreprocessingEdit::cancel() {
    edit_.cancel();
    view_.reset();
}
} // namespace qualpf::gui
