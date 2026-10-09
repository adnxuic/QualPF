#pragma once
#include "gui/state/search_session.h"
#include "qualpf/application/export/session_export.h"
#include "qualpf/application/export/atomic_export_writer.h"
#include <QString>

namespace qualpf::gui::exporting {
using namespace application; using namespace qualpf::gui;
using namespace application::exporting;
Result<qualpf::exporting::PatternExportSnapshot> prepare_session_export(const SessionStateView &,
                                                                        qualpf::exporting::PatternFormat);
} // namespace qualpf::gui::exporting
