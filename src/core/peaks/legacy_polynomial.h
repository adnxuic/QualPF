#pragma once
#include <array>
#include <span>
#include <vector>

namespace qualpf::preprocessing::detail {
// Three-column compatibility least squares. Local storage, legacy binary32
// bidiagonalization/QR and cutoff. Not a replacement for general S4 solvers.
std::array<float, 3> fit_log_quadratic(std::vector<std::array<float, 3>> matrix, std::span<const float> rhs);
} // namespace qualpf::preprocessing::detail
