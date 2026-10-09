#include "gui/export/session_export_adapter.h"
#include "qualpf/application/export/atomic_export_writer.h"
#include "qualpf/application/export/pattern_export_input.h"
#include "qualpf/application/export/session_export.h"
#include "gui/state/preprocessing_product.h"


namespace qualpf::gui::exporting {
using namespace application;
using namespace qualpf::gui;
using namespace application::exporting;
Result<qualpf::exporting::PatternExportSnapshot>
prepare_session_export(const SessionStateView &state, qualpf::exporting::PatternFormat format) {
    return prepareSessionExport(productState(state), state.projectMetadata["radtype"].toInt(), format);
}
} // namespace qualpf::gui::exporting
