#pragma once
#include "qualpf/domain/result.h"
#include <optional>
#include <string_view>
#include <vector>

namespace qualpf::application {
enum class PeakCoordinateRepresentation { two_theta, d_spacing };
struct ParsedPeakCoordinateEntry {
    PeakCoordinateRepresentation representation;
    double value;
    std::optional<double> original_d;
    std::size_t line;
};
// P8a: owning coordinates only, no inference, sorting, deduplication or
// Session.
Result<std::vector<ParsedPeakCoordinateEntry>>
parsePeakCoordinates(std::string_view bytes, PeakCoordinateRepresentation representation);
} // namespace qualpf::application
