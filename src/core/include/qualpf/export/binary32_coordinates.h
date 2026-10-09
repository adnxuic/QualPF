#pragma once
#include "qualpf/domain/result.h"
#include "qualpf/domain/units.h"
#include <cstdint>

namespace qualpf::exporting {
// Export-only Legacy transport. IEEE nearest/even, including gradual underflow;
// independent of the host rounding mode, libm, FMA and extended precision.
double export_binary32_transport(double) noexcept;

struct Binary32ExportCoordinate {
    double d_angstrom = 0;
    std::uint32_t two_theta_bits = 0, wavelength_bits = 0;
    std::uint32_t radians_bits = 0, half_angle_bits = 0;
    std::uint32_t sine_bits = 0, denominator_bits = 0, d_bits = 0;
    unsigned sine_precision_bits = 0;
};

// The frozen counts::dvalue operation sequence, for the isolated exporter only.
// This does not amend preprocessing::legacy_d or approve an export input domain.
Result<Binary32ExportCoordinate> resolve_export_binary32_d(TwoThetaDegrees, WavelengthAngstrom);
// Arithmetic trace, including defined IEEE nonfinite/sign results, for bounded
// review diagnostics. The supported-domain resolver above rejects unusable d;
// the serializer never receives a nonfinite/nonpositive d through Application.
Result<Binary32ExportCoordinate> evaluate_export_binary32_d(TwoThetaDegrees, WavelengthAngstrom);
} // namespace qualpf::exporting
