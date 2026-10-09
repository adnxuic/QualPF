#include "qualpf/application/export/atomic_export_writer.h"

namespace qualpf::application::exporting {
Status write_pattern_export_to(AtomicExportTarget &file, const qualpf::exporting::PatternExportSnapshot &snapshot,
                               qualpf::exporting::PatternFormat format) {
    auto bytes = qualpf::exporting::format_pattern(snapshot, format);
    if (!bytes)
        return Status{bytes.error()};
    if (!file.open())
        return Status{Error{ErrorCode::invalid_input, "export.target", file.error_message(), "path_or_open_failure"}};
    const auto &data = bytes.value();
    if (file.write(data) != static_cast<std::int64_t>(data.size())) {
        const auto message = file.error_message();
        file.cancel();
        return Status{Error{ErrorCode::invalid_input, "export.write", message, "write_failure"}};
    }
    if (!file.commit()) {
        const auto message = file.error_message();
        file.cancel();
        return Status{Error{ErrorCode::invalid_input, "export.commit", message, "commit_failure"}};
    }
    return Status{std::monostate{}};
}
} // namespace qualpf::application::exporting
