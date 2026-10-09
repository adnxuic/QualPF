#pragma once
#include "qualpf/domain/pattern.h"
#include "qualpf/domain/result.h"
#include <string>
#include <istream>

namespace qualpf::io {
struct PatternRead {
    Pattern pattern;
    bool file_wavelength = false;
    double nominal_step = 0;
};
// Pure owning result: no Session, GUI, native workspace, cwd change or output file.
// The text compatibility policy is frozen in pattern-reference-manifest.json.
Result<PatternRead> read_pattern(std::istream &, std::string extension, std::string source_id);
} // namespace qualpf::io
