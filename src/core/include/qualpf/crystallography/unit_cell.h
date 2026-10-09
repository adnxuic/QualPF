#pragma once

#include "qualpf/domain/result.h"
#include "qualpf/domain/units.h"

#include <array>
#include <cstdint>

namespace qualpf::crystallography {

struct UnitCellParameters {
    std::array<double, 3> lengths_angstrom;
    // alpha=(b,c), beta=(a,c), gamma=(a,b); always degrees, never cosines.
    std::array<double, 3> angles_degrees;
};

struct ReciprocalCellParameters {
    std::array<double, 3> lengths_inverse_angstrom;
    std::array<double, 3> angles_degrees;
};

struct Hkl {
    std::int32_t h, k, l;
};

// Row-major symmetric tensors. Reciprocal convention has no factor of 2*pi.
struct MetricTensor {
    std::array<double, 9> angstrom_squared;
};
struct ReciprocalMetricTensor {
    std::array<double, 9> inverse_angstrom_squared;
};
struct ReciprocalVectorNorm {
    double inverse_angstrom;
};
struct BraggAngleDegrees {
    double value;
};

// Validated, immutable value with no dependency on tables, symmetry or startup.
// Near singularity is measured by the dimensionless angular determinant
// (must exceed 64*double epsilon), not by the lengths' aspect ratio.
class UnitCell {
  public:
    [[nodiscard]] static Result<UnitCell> create(UnitCellParameters parameters);
    [[nodiscard]] static Result<UnitCell> from_reciprocal(ReciprocalCellParameters parameters);
    [[nodiscard]] const UnitCellParameters& parameters() const noexcept { return parameters_; }
    [[nodiscard]] const MetricTensor& metric() const noexcept { return metric_; }
    [[nodiscard]] const ReciprocalMetricTensor& reciprocal_metric() const noexcept {
        return reciprocal_metric_;
    }
    [[nodiscard]] double volume_angstrom_cubed() const noexcept { return volume_; }
    [[nodiscard]] double reciprocal_volume_inverse_angstrom_cubed() const noexcept {
        return 1.0 / volume_;
    }
    [[nodiscard]] Result<ReciprocalCellParameters> reciprocal() const;
    [[nodiscard]] Result<ReciprocalVectorNorm> reciprocal_vector_norm(Hkl hkl) const;
    [[nodiscard]] Result<DSpacingAngstrom> d_spacing(Hkl hkl) const;

  private:
    UnitCell() = default;
    UnitCellParameters parameters_{};
    MetricTensor metric_{};
    ReciprocalMetricTensor reciprocal_metric_{};
    // Inverse of a lower Cholesky factor; maps explicit hkl to Cartesian q.
    std::array<double, 9> inverse_factor_{};
    double volume_ = 0.0;
};

[[nodiscard]] Result<BraggAngleDegrees> bragg_angle(DSpacingAngstrom d,
                                                    WavelengthAngstrom wavelength);
[[nodiscard]] Result<TwoThetaDegrees> two_theta(DSpacingAngstrom d, WavelengthAngstrom wavelength);
[[nodiscard]] Result<DSpacingAngstrom> d_spacing(TwoThetaDegrees angle,
                                                 WavelengthAngstrom wavelength);

} // namespace qualpf::crystallography
