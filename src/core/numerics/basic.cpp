#include "qualpf/numerics/basic.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numeric>

namespace qualpf::numerics {
namespace {
Error invalid(const char *field, const char *message) { return {ErrorCode::invalid_input, field, message}; }
Error overflow(const char *field) { return {ErrorCode::numerical_failure, field, "Non-finite numerical result"}; }
bool finite(std::span<const double> values) {
    return std::all_of(values.begin(), values.end(), [](double x) { return std::isfinite(x); });
}
// Neumaier accumulation also handles a term larger than the running sum.
struct Accumulator {
    double value = 0;
    double correction = 0;
    void add(double x) {
        const double next = value + x;
        correction += std::abs(value) >= std::abs(x) ? (value - next) + x : (x - next) + value;
        value = next;
    }
    double total() const { return value + correction; }
};
} // namespace

Result<DSpacingAngstrom> to_d_spacing(TwoThetaDegrees angle, WavelengthAngstrom wavelength) {
    if (!std::isfinite(angle.value) || angle.value <= 0 || angle.value > 180)
        return Result<DSpacingAngstrom>{invalid("angle", "Require 0 < two_theta <= 180 degrees")};
    if (!std::isfinite(wavelength.value) || wavelength.value <= 0)
        return Result<DSpacingAngstrom>{invalid("wavelength", "Require a positive finite wavelength")};
    const double value = wavelength.value / (2 * std::sin(radians(angle.value) / 2));
    if (!std::isfinite(value) || value <= 0)
        return Result<DSpacingAngstrom>{overflow("spacing")};
    return Result<DSpacingAngstrom>{DSpacingAngstrom{value}};
}

Result<TwoThetaDegrees> to_two_theta(DSpacingAngstrom spacing, WavelengthAngstrom wavelength) {
    if (!std::isfinite(spacing.value) || spacing.value <= 0 || !std::isfinite(wavelength.value) ||
        wavelength.value <= 0)
        return Result<TwoThetaDegrees>{invalid("bragg", "Require positive finite spacing and wavelength")};
    const double ratio = (wavelength.value / spacing.value) / 2;
    if (ratio > 1)
        return Result<TwoThetaDegrees>{invalid("spacing", "Spacing is below wavelength / 2")};
    const double value = 2 * degrees(std::asin(ratio));
    if (!std::isfinite(value) || value <= 0)
        return Result<TwoThetaDegrees>{overflow("angle")};
    return Result<TwoThetaDegrees>{TwoThetaDegrees{value}};
}

Result<std::int32_t> nearest_integer(double value) {
    const double rounded = std::round(value);
    if (!std::isfinite(rounded) || rounded < std::numeric_limits<std::int32_t>::min() ||
        rounded > std::numeric_limits<std::int32_t>::max())
        return Result<std::int32_t>{invalid("value", "Rounded value is outside int32 range")};
    return Result<std::int32_t>{static_cast<std::int32_t>(rounded)};
}
Result<float> to_float32(double value) {
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
        return Result<float>{invalid("value", "Value is outside finite float32 range")};
    return Result<float>{static_cast<float>(value)};
}
Result<double> sum(std::span<const double> values) {
    if (!finite(values))
        return Result<double>{invalid("values", "Require finite values")};
    Accumulator result;
    for (double x : values)
        result.add(x);
    if (!std::isfinite(result.total()))
        return Result<double>{overflow("sum")};
    return Result<double>{result.total()};
}
Result<double> dot(std::span<const double> a, std::span<const double> b) {
    if (a.size() != b.size() || !finite(a) || !finite(b))
        return Result<double>{invalid("vectors", "Require equal lengths and finite values")};
    Accumulator result;
    for (std::size_t i = 0; i < a.size(); ++i)
        result.add(a[i] * b[i]);
    if (!std::isfinite(result.total()))
        return Result<double>{overflow("dot")};
    return Result<double>{result.total()};
}
Result<double> norm(std::span<const double> values) {
    if (!finite(values))
        return Result<double>{invalid("values", "Require finite values")};
    double result = 0;
    for (double x : values)
        result = std::hypot(result, x);
    if (!std::isfinite(result))
        return Result<double>{overflow("norm")};
    return Result<double>{result};
}
Result<Statistics> statistics(std::span<const double> values, Variance kind) {
    if ((kind != Variance::sample && kind != Variance::population) || values.empty() ||
        (kind == Variance::sample && values.size() < 2) || !finite(values))
        return Result<Statistics>{invalid("values", "Require finite data and enough samples for variance")};
    // Shift first to preserve small spreads about large offsets. Divide before
    // accumulation to avoid overflowing the sum when the mean is representable.
    const double n = static_cast<double>(values.size());
    Accumulator offsets;
    for (double x : values)
        offsets.add((x - values.front()) / n);
    const double mean = values.front() + offsets.total();
    Accumulator squares;
    Accumulator deviations;
    const double denominator = kind == Variance::sample ? n - 1 : n;
    for (double x : values) {
        const double d = x - mean;
        deviations.add(d / n);
        squares.add((d / std::sqrt(denominator)) * (d / std::sqrt(denominator)));
    }
    const double correction = deviations.total();
    const double corrected = squares.total() - correction * correction * (n / denominator);
    if (!std::isfinite(mean) || !std::isfinite(squares.total()) || !std::isfinite(correction) ||
        !std::isfinite(corrected))
        return Result<Statistics>{overflow("statistics")};
    return Result<Statistics>{Statistics{values.size(), mean, std::max(0.0, corrected)}};
}
Result<std::vector<std::size_t>> sort_indices(std::span<const double> values) {
    if (!finite(values))
        return Result<std::vector<std::size_t>>{invalid("values", "Require finite sort keys")};
    std::vector<std::size_t> order(values.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) { return values[a] < values[b]; });
    return Result<std::vector<std::size_t>>{std::move(order)};
}
Result<std::vector<double>> linspace(double first, double last, std::size_t count) {
    if (!std::isfinite(first) || !std::isfinite(last) || count == 0 || (count == 1 && first != last))
        return Result<std::vector<double>>{
            invalid("linspace", "Require finite endpoints, positive count and equal endpoints for one point")};
    std::vector<double> values(count, first);
    for (std::size_t i = 1; i < count; ++i)
        values[i] = std::lerp(first, last, static_cast<double>(i) / static_cast<double>(count - 1));
    return Result<std::vector<double>>{std::move(values)};
}
Result<std::size_t> nearest_index(std::span<const double> grid, double query) {
    if (grid.empty() || !finite(grid) || !std::isfinite(query))
        return Result<std::size_t>{invalid("grid", "Require a nonempty finite grid and finite query")};
    for (std::size_t i = 1; i < grid.size(); ++i)
        if (grid[i] <= grid[i - 1])
            return Result<std::size_t>{invalid("grid", "Knots must increase strictly")};
    const auto found = std::lower_bound(grid.begin(), grid.end(), query);
    if (found == grid.begin())
        return Result<std::size_t>{0};
    if (found == grid.end())
        return Result<std::size_t>{grid.size() - 1};
    const auto upper = static_cast<std::size_t>(found - grid.begin());
    // midpoint avoids overflow in distance subtraction for extreme endpoints.
    return Result<std::size_t>{query < std::midpoint(grid[upper - 1], grid[upper]) ? upper - 1 : upper};
}
Result<std::size_t> locate_interval(std::span<const double> grid, double query, OutOfRange policy) {
    if (grid.size() < 2 || !finite(grid) || !std::isfinite(query) ||
        (policy != OutOfRange::reject && policy != OutOfRange::clamp && policy != OutOfRange::extrapolate))
        return Result<std::size_t>{
            invalid("grid", "Require at least two finite knots, a finite query and valid policy")};
    const bool ascending = grid.back() > grid.front();
    for (std::size_t i = 1; i < grid.size(); ++i)
        if (ascending ? grid[i] <= grid[i - 1] : grid[i] >= grid[i - 1])
            return Result<std::size_t>{invalid("grid", "Knots must be strictly monotone")};
    if (query < std::min(grid.front(), grid.back()) || query > std::max(grid.front(), grid.back())) {
        if (policy == OutOfRange::reject)
            return Result<std::size_t>{invalid("query", "Query lies outside the grid")};
    }
    const auto it = ascending ? std::upper_bound(grid.begin(), grid.end(), query)
                              : std::upper_bound(grid.begin(), grid.end(), query, std::greater<double>{});
    const auto position = static_cast<std::size_t>(it - grid.begin());
    return Result<std::size_t>{position == 0 ? 0 : std::min(position - 1, grid.size() - 2)};
}
Result<double> trapezoid(std::span<const double> x, std::span<const double> y, SinglePointIntegral single) {
    if (x.empty() || x.size() != y.size() || !finite(x) || !finite(y) ||
        (single != SinglePointIntegral::zero && single != SinglePointIntegral::legacy_sample_value))
        return Result<double>{invalid("samples", "Require equal nonempty finite arrays and valid singleton policy")};
    if (x.size() == 1)
        return Result<double>{single == SinglePointIntegral::zero ? 0 : y.front()};
    Accumulator result;
    for (std::size_t i = 1; i < x.size(); ++i) {
        if (x[i] <= x[i - 1])
            return Result<double>{invalid("x", "Integration grid must increase strictly")};
        result.add((x[i] - x[i - 1]) * std::midpoint(y[i], y[i - 1]));
    }
    if (!std::isfinite(result.total()))
        return Result<double>{overflow("integral")};
    return Result<double>{result.total()};
}
} // namespace qualpf::numerics
