#pragma once
#include "qualpf/profile/pfsm.h"
#include <array>

namespace qualpf::profile::detail {
using IlCoefficients = std::array<double, 32>;
enum class SimdResult { complete, cancelled, fallback };
// This detector must be compiled for the baseline ISA, independently of AVX2.
bool avx2Available() noexcept;
// Call only after avx2Available(), coordinate validation and existing domain guards.
// Direct IL accepts complete groups of four; the baseline caller owns its remainder.
SimdResult ilAvx2(std::span<const double>, std::span<const ProfileLine>, double shift, double factor,
                  std::span<double>, std::stop_token);
SimdResult cachedIlAvx2(std::span<const double>, double lower, double step, double shift,
                        std::span<const IlCoefficients>, std::span<double>, std::stop_token);
} // namespace qualpf::profile::detail
