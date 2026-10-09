#include "qualpf/application/export/session_export.h"
#include "qualpf/application/preprocessing_evaluation.h"
namespace qualpf::application::exporting {
Result<qualpf::exporting::PatternExportSnapshot>
prepareSessionExport(const ProductState &state, int radiationCode, qualpf::exporting::PatternFormat format) {
    ExportMetadata metadata{radiationCode, state.editing};
    metadata.background_control_point_count =
        hasActiveBackground(state.pattern) && state.preprocessing.background_points
            ? state.preprocessing.background_points->size()
            : 0;
    const auto method = state.preprocessing.background.value_or(preprocessing::BackgroundOptions{}).method;
    metadata.background_method = method == preprocessing::BackgroundMethod::none
                                     ? BackgroundExportMethod::none
                                     : BackgroundExportMethod::enabled;
    return prepare_pattern_export(state.pattern, metadata, format);
}
} // namespace qualpf::application::exporting
