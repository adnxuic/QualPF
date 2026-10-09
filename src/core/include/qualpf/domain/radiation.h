#pragma once

#include "qualpf/domain/units.h"

namespace qualpf::radiation {
// Canonical Cu radiation values specified for QualPF on 2026-09-26.
// Angstrom; K-alpha is the 2:1 weighted average, rounded to eight decimals.
inline constexpr double CU_KA1_WAVE = 1.54059307;
inline constexpr double CU_KA2_WAVE = 1.54442756;
inline constexpr double CU_KA_WAVE = 1.54187123;

inline constexpr double default_wavelength_angstrom = CU_KA1_WAVE;
inline constexpr WavelengthAngstrom default_wavelength{default_wavelength_angstrom};
// The retained Pattern IO ABI transports wavelengths as Fortran REAL / C float.
inline constexpr double transported_default_wavelength =
    static_cast<double>(static_cast<float>(default_wavelength_angstrom));
} // namespace qualpf::radiation
