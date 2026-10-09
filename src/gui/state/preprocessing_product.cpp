#include "gui/state/preprocessing_product.h"
#include "gui/state/workspace_context.h"
#include <algorithm>
#include <cmath>

namespace qualpf::gui {
using namespace application;
bool cppPreprocessingProduct() { return qEnvironmentVariable("QUALPF_PREPROCESSING_BACKEND", "cpp") == "cpp"; }
Status peakEditEntryStatus(qualpf::gui::WorkspaceContext &context) {
    if (context.workspace().searches().closing())
        return Status{Error{ErrorCode::cancelled,"preprocessing","Workspace is closing","closing"}};
    if (context.searches().isSearchRunning())
        return Status{Error{ErrorCode::invalid_input,"peaks.operation","Finish the running Search first","search_in_progress"}};
    if (context.searchSession().snapshot()->editing)
        return Status{Error{ErrorCode::stale_snapshot,"preprocessing","Finish the open edit first"}};
    return Status{std::monostate{}};
}
Status applyPreprocessing(SessionView &session, PreprocessingAction action) {
    auto request = automaticPreprocessingRequest(session.canonicalSession().snapshot(), action);
    if (!request) return Status{request.error()};
    PreprocessingEdit edit(session);
    auto prepared = edit.preview(request.value());
    if (!prepared) return prepared;
    return edit.commit();
}
Status editBackgroundPoint(SessionView &session, bool add, double x, double y, bool redetect) {
    auto request = backgroundPointRequest(session.canonicalSession().snapshot(), add, x, y, redetect);
    if (!request) return Status{request.error()};
    PreprocessingEdit edit(session);
    auto prepared = edit.preview(request.value());
    if (!prepared) return prepared;
    return edit.commit();
}
} // namespace qualpf::gui
