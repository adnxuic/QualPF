#include "gui/state/preprocessing_product.h"
#include "gui/state/workspace_context.h"
#include <algorithm>
#include <cmath>
#include <QStringList>

namespace qualpf::gui {
using namespace application;
QString peakRejectionSummary(const PreprocessingConfiguration &configuration) {
    return configuration.rejected_candidates.empty() ? QString() :
        QObject::tr("Skipped %1 unmeasurable candidates").arg(configuration.rejected_candidates.size());
}
QString peakRejectionDetails(const PreprocessingConfiguration &configuration) {
    QStringList details;
    for (const auto &r : configuration.rejected_candidates)
        details.append(QObject::tr("%1: candidate %2, sample %3, 2\u03b8 = %4\u00b0: coincident baseline endpoints")
            .arg(r.stage == preprocessing::PeakDetectionStage::initial ? QObject::tr("Initial search") : QObject::tr("Formal search"))
            .arg(r.candidate_index).arg(r.sample_index).arg(r.two_theta.value, 0, 'f', 6));
    return details.join('\n');
}
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
    if (request.value().peaks && !request.value().fwhm)
        request.value().fwhm = session.snapshot()->preprocessing.fwhm.value_or(FwhmConfiguration{});
    PreprocessingEdit edit(session);
    auto prepared = edit.preview(request.value());
    if (!prepared) return prepared;
    return edit.commit();
}
Status editBackgroundPoint(SessionView &session, bool add, double x, double y, bool redetect) {
    auto request = backgroundPointRequest(session.canonicalSession().snapshot(), add, x, y, redetect);
    if (!request) return Status{request.error()};
    if (request.value().peaks)
        request.value().fwhm = session.snapshot()->preprocessing.fwhm.value_or(FwhmConfiguration{});
    PreprocessingEdit edit(session);
    auto prepared = edit.preview(request.value());
    if (!prepared) return prepared;
    return edit.commit();
}
} // namespace qualpf::gui
