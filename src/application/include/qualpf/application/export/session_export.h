#pragma once
#include "qualpf/application/export/pattern_export_input.h"
#include "qualpf/application/product_session.h"
namespace qualpf::application::exporting {
Result<qualpf::exporting::PatternExportSnapshot> prepareSessionExport(const ProductState &, int radiationCode,
                                                                      qualpf::exporting::PatternFormat);
}
