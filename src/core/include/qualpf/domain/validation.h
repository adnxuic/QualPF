#pragma once

#include "qualpf/domain/search.h"

#include <vector>

namespace qualpf {

using ValidationIssues = std::vector<Error>;
// Structural checks only. Never repair, normalize, interpolate, clamp or sort.
[[nodiscard]] ValidationIssues validate(const Pattern& value);
[[nodiscard]] ValidationIssues validate(const PeakSet& value);
[[nodiscard]] ValidationIssues validate(const ReferenceCard& value);
[[nodiscard]] ValidationIssues validate(const SearchParameters& value);
[[nodiscard]] ValidationIssues validate(const SearchResults& value);
[[nodiscard]] ValidationIssues validate(const ScoreInput& value);

} // namespace qualpf
