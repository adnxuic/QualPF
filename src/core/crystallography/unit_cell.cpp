#include "qualpf/crystallography/unit_cell.h"
#include "phase_sincos.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace qualpf::crystallography {
namespace {
constexpr double radians = std::numbers::pi / 180.0;

Error invalid(const char* field, const char* reason, const char* message) {
    return {ErrorCode::invalid_input, field, message, reason};
}
Error range_error(const char* field, const char* reason) {
    return {ErrorCode::numerical_failure, field,
            "Result is outside the supported floating-point range", reason};
}
bool positive(double x) { return std::isfinite(x) && x > 0.0; }
double sine(double argument) { return detail::phase_phasor(argument).imag(); }
double cosine(double degrees) { return degrees == 90.0 ? 0.0 : detail::phase_phasor(degrees * radians).real(); }

// Avoid overflow/underflow in intermediate products of very unequal lengths.
double product(std::array<double, 4> values) {
    int exponent = 0;
    double mantissa = 1.0;
    for (double v : values) {
        int e = 0;
        mantissa *= std::frexp(v, &e);
        exponent += e;
    }
    return std::ldexp(mantissa, exponent);
}
} // namespace

Result<UnitCell> UnitCell::create(UnitCellParameters p) {
    for (double x : p.lengths_angstrom)
        if (!positive(x))
            return Result<UnitCell>(invalid("cell.lengths", "invalid_cell_length",
                                            "Lengths must be positive and finite"));
    for (double x : p.angles_degrees)
        if (!std::isfinite(x) || x <= 0.0 || x >= 180.0)
            return Result<UnitCell>(invalid("cell.angles", "invalid_cell_angle",
                                            "Angles must lie strictly between 0 and 180 degrees"));
    const auto [alpha, beta, gamma] = p.angles_degrees;
    const double half_sum = (alpha + beta + gamma) * 0.5;
    const bool impossible = alpha + beta <= gamma || alpha + gamma <= beta || beta + gamma <= alpha ||
                            half_sum >= 180.0;
    // The half-angle identity is more stable near degeneracy than subtracting
    // 1 + 2*cos(alpha)*cos(beta)*cos(gamma) - sum(cos(angle)^2).
    // Impossible parameters retain their original error without entering the
    // private bounded trigonometric owner outside its supported argument range.
    const double determinant = impossible ? 0.0 :
        4.0 * sine(half_sum * radians) * sine((half_sum - alpha) * radians) *
        sine((half_sum - beta) * radians) * sine((half_sum - gamma) * radians);
    if (impossible || determinant <= 64.0 * std::numeric_limits<double>::epsilon())
        return Result<UnitCell>(
            invalid("cell.angles", "degenerate_cell",
                    "Cell is impossible, singular or too close to angular degeneracy"));

    UnitCell cell;
    cell.parameters_ = p;
    const auto [a, b, c] = p.lengths_angstrom;
    const double ca = cosine(alpha), cb = cosine(beta), cg = cosine(gamma);
    const double sg = sine(gamma * radians);
    const double t = (ca - cb * cg) / sg;
    const double z = std::sqrt(determinant) / sg;
    cell.metric_.angstrom_squared = {a * a,      a * b * cg, a * c * cb, a * b * cg, b * b,
                                     b * c * ca, a * c * cb, b * c * ca, c * c};
    // Solve three triangular systems rather than subtract nearly equal cofactors.
    auto& f = cell.inverse_factor_;
    f[0] = 1.0 / a;
    f[3] = -cg * f[0] / sg;
    f[4] = (1.0 / b) / sg;
    f[6] = (-cb * f[0] - t * f[3]) / z;
    f[7] = -t * f[4] / z;
    f[8] = (1.0 / c) / z;
    auto& g = cell.reciprocal_metric_.inverse_angstrom_squared;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            g[3 * i + j] = f[i] * f[j] + f[3 + i] * f[3 + j] + f[6 + i] * f[6 + j];
    cell.volume_ = product({a, b, c, std::sqrt(determinant)});
    if (!positive(cell.volume_) || !positive(1.0 / cell.volume_))
        return Result<UnitCell>(range_error("cell", "cell_range"));
    for (int i = 0; i < 9; ++i)
        if (!std::isfinite(cell.metric_.angstrom_squared[i]) || !std::isfinite(g[i]) ||
            (i % 4 == 0 && (!positive(cell.metric_.angstrom_squared[i]) || !positive(g[i]))))
            return Result<UnitCell>(range_error("cell.metric", "cell_range"));
    return Result<UnitCell>(cell);
}

Result<ReciprocalCellParameters> UnitCell::reciprocal() const {
    ReciprocalCellParameters p{};
    std::array<std::array<double, 3>, 3> directions{};
    for (int i = 0; i < 3; ++i) {
        const auto& f = inverse_factor_;
        const double length = std::hypot(f[i], f[3 + i], f[6 + i]);
        p.lengths_inverse_angstrom[i] = length;
        directions[i] = {f[i] / length, f[3 + i] / length, f[6 + i] / length};
    }
    for (int i = 0; i < 3; ++i) {
        const auto& u = directions[(i + 1) % 3];
        const auto& v = directions[(i + 2) % 3];
        const double cross = std::hypot(u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                                        u[0] * v[1] - u[1] * v[0]);
        const double dot = u[0] * v[0] + u[1] * v[1] + u[2] * v[2];
        p.angles_degrees[i] = std::atan2(cross, dot) / radians;
    }
    // A valid direct representation need not yield a reliably representable
    // reciprocal parameterization at the very edge of the angular guard.
    const auto check = create({p.lengths_inverse_angstrom, p.angles_degrees});
    if (!check)
        return Result<ReciprocalCellParameters>(check.error());
    return Result<ReciprocalCellParameters>(p);
}

Result<UnitCell> UnitCell::from_reciprocal(ReciprocalCellParameters p) {
    const auto reciprocal_cell = create({p.lengths_inverse_angstrom, p.angles_degrees});
    if (!reciprocal_cell)
        return Result<UnitCell>(reciprocal_cell.error());
    const auto direct = reciprocal_cell.value().reciprocal();
    if (!direct)
        return Result<UnitCell>(direct.error());
    return create({direct.value().lengths_inverse_angstrom, direct.value().angles_degrees});
}

Result<ReciprocalVectorNorm> UnitCell::reciprocal_vector_norm(Hkl hkl) const {
    if (hkl.h == 0 && hkl.k == 0 && hkl.l == 0)
        return Result<ReciprocalVectorNorm>(
            invalid("hkl", "zero_hkl", "The zero vector has no reflection spacing"));
    const double h = hkl.h, k = hkl.k, l = hkl.l;
    const auto& f = inverse_factor_;
    const double norm = std::hypot(f[0] * h, f[3] * h + f[4] * k, f[6] * h + f[7] * k + f[8] * l);
    if (!positive(norm))
        return Result<ReciprocalVectorNorm>(range_error("hkl", "reflection_range"));
    return Result<ReciprocalVectorNorm>(ReciprocalVectorNorm{norm});
}

Result<DSpacingAngstrom> UnitCell::d_spacing(Hkl hkl) const {
    const auto norm = reciprocal_vector_norm(hkl);
    if (!norm)
        return Result<DSpacingAngstrom>(norm.error());
    const double d = 1.0 / norm.value().inverse_angstrom;
    if (!positive(d))
        return Result<DSpacingAngstrom>(range_error("hkl", "reflection_range"));
    return Result<DSpacingAngstrom>(DSpacingAngstrom{d});
}

Result<BraggAngleDegrees> bragg_angle(DSpacingAngstrom d, WavelengthAngstrom wavelength) {
    if (!positive(wavelength.value))
        return Result<BraggAngleDegrees>(
            invalid("wavelength", "invalid_wavelength", "Wavelength must be positive and finite"));
    if (!positive(d.value))
        return Result<BraggAngleDegrees>(
            invalid("d", "invalid_d_spacing", "Spacing must be positive and finite"));
    // Scale before division: even subnormal inputs retain their ratio, while
    // neither 2*d nor wavelength/2 is required to be representable.
    int ew = 0, ed = 0;
    const double mw = std::frexp(wavelength.value, &ew), md = std::frexp(d.value, &ed);
    const double ratio = std::ldexp(mw / md, ew - ed - 1);
    if (ratio > 1.0)
        return Result<BraggAngleDegrees>(
            invalid("d", "bragg_domain", "Wavelength exceeds twice the spacing"));
    const double theta = std::asin(ratio) / radians;
    if (!positive(theta))
        return Result<BraggAngleDegrees>(range_error("angle", "bragg_range"));
    return Result<BraggAngleDegrees>(BraggAngleDegrees{theta});
}

Result<TwoThetaDegrees> two_theta(DSpacingAngstrom d, WavelengthAngstrom wavelength) {
    const auto theta = bragg_angle(d, wavelength);
    if (!theta)
        return Result<TwoThetaDegrees>(theta.error());
    return Result<TwoThetaDegrees>(TwoThetaDegrees{2.0 * theta.value().value});
}

Result<DSpacingAngstrom> d_spacing(TwoThetaDegrees angle, WavelengthAngstrom wavelength) {
    if (!positive(wavelength.value))
        return Result<DSpacingAngstrom>(
            invalid("wavelength", "invalid_wavelength", "Wavelength must be positive and finite"));
    if (!positive(angle.value) || angle.value > 180.0)
        return Result<DSpacingAngstrom>(
            invalid("angle", "invalid_two_theta", "Two theta must lie in (0,180] degrees"));
    const double denominator = 2.0 * std::sin(angle.value * (radians * 0.5));
    const double d = wavelength.value / denominator;
    if (!positive(d))
        return Result<DSpacingAngstrom>(range_error("d", "bragg_range"));
    return Result<DSpacingAngstrom>(DSpacingAngstrom{d});
}
} // namespace qualpf::crystallography
