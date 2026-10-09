#include "gui/export/pattern_export_service.h"
#include "qualpf/application/export/atomic_export_writer.h"
#include "gui/export/session_export_adapter.h"

namespace qualpf::gui::exporting {
using namespace application; using namespace qualpf::gui;
using namespace application::exporting;
Result<qualpf::exporting::PatternExportSnapshot>
PatternExportService::snapshot(qualpf::exporting::PatternFormat format) const {
    if (!source_)
        return Result<qualpf::exporting::PatternExportSnapshot>{
            Error{ErrorCode::missing_data, "export.session", "Missing canonical export source", "missing_snapshot"}};
    return prepare_session_export(*source_, format);
}
Status PatternExportService::save(const QString &target, qualpf::exporting::PatternFormat format) const {
    if (target.isEmpty())
        return Status{Error{ErrorCode::cancelled, "export.target", "No export target", "empty_target"}};
    auto prepared = snapshot(format);
    if (!prepared)
        return Status{prepared.error()};
    return writePatternExport(factory_, target.toStdString(), prepared.value(), format);
}
Status PatternExportService::saveTo(AtomicExportTarget &target, qualpf::exporting::PatternFormat format) const {
    auto prepared = snapshot(format);
    if (!prepared)
        return Status{prepared.error()};
    return write_pattern_export_to(target, prepared.value(), format);
}
} // namespace qualpf::gui::exporting
