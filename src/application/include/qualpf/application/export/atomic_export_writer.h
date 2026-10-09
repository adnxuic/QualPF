#pragma once
#include "qualpf/export/pattern_export.h"
#include <cstdint>
#include <string_view>

namespace qualpf::application::exporting {
// Application I/O port. A failed/cancelled transaction must preserve the previous
// target. File choice, overwrite confirmation and recent directories stay in GUI.
class AtomicExportTarget {
  public:
    virtual ~AtomicExportTarget() = default;
    virtual bool open() = 0;
    virtual std::int64_t write(std::string_view) = 0;
    virtual bool commit() = 0;
    virtual void cancel() noexcept = 0;
    virtual std::string error_message() const = 0;
};
Status write_pattern_export_to(AtomicExportTarget &, const qualpf::exporting::PatternExportSnapshot &,
                               qualpf::exporting::PatternFormat);
} // namespace qualpf::application::exporting
