#pragma once
#include "legacy_internal.h"

namespace qualpf::preprocessing::detail {
// Phase31C P1: preserve the defined native final-store representation.
// This private seam requires the same valid arithmetic inputs as average().
// Consumer validation, error propagation and publication remain caller-owned.
Vector average_compatibility(const Vector &input, int width);
} // namespace qualpf::preprocessing::detail
