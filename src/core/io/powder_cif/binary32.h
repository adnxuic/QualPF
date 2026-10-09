#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

// Binary32 operations serve powder import compatibility; Pattern stays binary64.
namespace qualpf::io::powder_cif::binary32 {
using U = std::uint32_t;
U add(U, U);
U mul(U, U);
U divide(U, U);
U decode(const std::string &);
U certifiedAsin(U);
// Shared certified enclosure, canonical binary64 principal asin in RNE.
// Preserves signed zero; invalid arguments return a fixed quiet NaN.
// Never returns an uncertified approximation; resource exhaustion may throw.
std::uint64_t certifiedAsin64(std::uint64_t);
std::uint64_t widen(U);
U index(std::size_t);
bool finite(U) noexcept;
bool less(U, U) noexcept;
double value(U);
} // namespace qualpf::io::powder_cif::binary32
