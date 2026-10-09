#pragma once

#include <bit>
#include <complex>
#include <cstdint>
#include <limits>

namespace qualpf::crystallography::detail {
struct PhaseSincosBits {
  std::uint64_t sine, cosine;
};

// Mathematical binary64 round-to-nearest-even of the already formed phase.
// The caller reduces cycles first, so |phase| <= binary64 pi. Invalid private
// inputs throw; normal allocation failures propagate before result publication.
// force_rational exercises the same production fallback in contract tests.
[[nodiscard]] PhaseSincosBits
phase_sincos_bits(std::uint64_t phase_bits, bool force_rational = false);

[[nodiscard]] inline std::complex<double> phase_phasor(double phase) {
  static_assert(sizeof(double) == sizeof(std::uint64_t) &&
                std::numeric_limits<double>::is_iec559);
  const auto result = phase_sincos_bits(std::bit_cast<std::uint64_t>(phase));
  return {std::bit_cast<double>(result.cosine),
          std::bit_cast<double>(result.sine)};
}
} // namespace qualpf::crystallography::detail
