#pragma once
#include "qualpf/domain/pattern.h"
#include "qualpf/export/pattern_export.h"

namespace qualpf::application::exporting {
enum class BackgroundExportMethod { none, enabled };
struct ExportMetadata {
    int radiation_code = 0; // Existing projectMetadata.toInt() contract.
    bool editing = false;
    // Absent canonical options retain the native new-dataset Chebyshev default.
    // Count means control points, never the number of aligned background samples.
    BackgroundExportMethod background_method = BackgroundExportMethod::enabled;
    std::size_t background_control_point_count = 0;
};
// Qt/Fortran-free value adapter; no numerical state publication.
Result<qualpf::exporting::PatternExportSnapshot> prepare_pattern_export(const Pattern &, ExportMetadata,
                                                                        qualpf::exporting::PatternFormat);
} // namespace qualpf::application::exporting
