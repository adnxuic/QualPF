#pragma once

#include "qualpf/domain/result.h"
#include "qualpf/domain/units.h"

#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>
#include <vector>

namespace qualpf::numerics {

// All foundation operations use binary64 arithmetic. They do not implement
// NumericalPolicy::legacy_mixed_precision or read any session/global state.
constexpr double radians(double degrees) noexcept { return degrees * (std::numbers::pi / 180.0); }
constexpr double degrees(double radians) noexcept { return radians * (180.0 / std::numbers::pi); }

Result<DSpacingAngstrom> to_d_spacing(TwoThetaDegrees angle, WavelengthAngstrom wavelength);
Result<TwoThetaDegrees> to_two_theta(DSpacingAngstrom spacing, WavelengthAngstrom wavelength);
// Fortran NINT semantics: nearest integer, halfway away from zero; checked range.
Result<std::int32_t> nearest_integer(double value);
// Finite values only. Underflow/rounding to float is permitted; overflow is not.
Result<float> to_float32(double value);

Result<double> sum(std::span<const double> values); // compensated; empty sum = 0
Result<double> dot(std::span<const double> a, std::span<const double> b);
Result<double> norm(std::span<const double> values); // scaled Euclidean norm

struct Statistics {
    std::size_t count;
    double mean;
    double variance;
};
enum class Variance { population, sample };
Result<Statistics> statistics(std::span<const double> values, Variance kind = Variance::sample);

// Stable ascending permutation, zero-based; does not mutate the input.
Result<std::vector<std::size_t>> sort_indices(std::span<const double> values);
// Nearest point on a strictly increasing grid; halfway ties select the higher
// index, matching arrayutil::clocate. Outside queries select an endpoint.
Result<std::size_t> nearest_index(std::span<const double> grid, double query);
Result<std::vector<double>> linspace(double first, double last, std::size_t count);

enum class OutOfRange { reject, clamp, extrapolate };
// Validates a strictly monotone (ascending OR descending) finite grid.
// Last endpoint belongs to the last segment; outside uses an end segment only
// when policy permits it. Duplicate knots/one-point grids are rejected.
Result<std::size_t> locate_interval(std::span<const double> grid, double query, OutOfRange policy = OutOfRange::reject);
enum class SinglePointIntegral { zero, legacy_sample_value };
// Integral over the entire ascending grid. Subranges use explicit subspans.
Result<double> trapezoid(std::span<const double> x, std::span<const double> y,
                         SinglePointIntegral single = SinglePointIntegral::zero);

} // namespace qualpf::numerics
