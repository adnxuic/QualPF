#pragma once
#include "qualpf/io/pattern_reader.h"
#include <filesystem>
namespace qualpf::io {
// File access provider. The Core overload accepts memory/streams only.
Result<PatternRead> read_pattern(const std::filesystem::path &);
} // namespace qualpf::io
