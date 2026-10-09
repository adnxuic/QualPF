#include "qualpf/numerics/interpolation.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace qualpf::numerics {
namespace {
Status validate_table(std::span<const double> x, std::span<const double> y) {
    if (x.size() < 2 || x.size() != y.size())
        return Status{Error{ErrorCode::invalid_input, "table", "Require at least two paired knots"}};
    for (std::size_t i = 0; i < x.size(); ++i) {
        if (!std::isfinite(x[i]) || !std::isfinite(y[i]) ||
            (i && (x[i] <= x[i - 1] || !std::isfinite(x[i] - x[i - 1]))))
            return Status{Error{ErrorCode::invalid_input, "table",
                                "Require finite values and strictly increasing finite intervals"}};
    }
    return Status{std::monostate{}};
}
Error failure() {
    return {ErrorCode::numerical_failure, "interpolation", "Interpolation arithmetic exceeded finite range"};
}
} // namespace

Interpolator::Interpolator(std::vector<double> x, std::vector<double> y, std::vector<double> second)
    : x_(std::move(x)), y_(std::move(y)), second_(std::move(second)) {}

Result<Interpolator> linear_interpolator(std::span<const double> x, std::span<const double> y) {
    const auto status = validate_table(x, y);
    if (!status)
        return Result<Interpolator>{status.error()};
    return Result<Interpolator>{Interpolator{{x.begin(), x.end()}, {y.begin(), y.end()}, {}}};
}
Result<Interpolator> cubic_spline(std::span<const double> x, std::span<const double> y, SplineBoundary boundary) {
    const auto status = validate_table(x, y);
    if (!status)
        return Result<Interpolator>{status.error()};
    for (auto derivative : {boundary.first_derivative, boundary.last_derivative})
        if (derivative && !std::isfinite(*derivative))
            return Result<Interpolator>{
                Error{ErrorCode::invalid_input, "boundary", "Endpoint derivatives must be finite"}};

    const auto n = x.size();
    std::vector<double> lower(n, 0), diagonal(n, 1), upper(n, 0), rhs(n, 0);
    // Row-normalized equations for second derivatives. The strictly increasing
    // grid makes this tridiagonal system strictly diagonally dominant; a simple
    // O(n) elimination is appropriate and avoids constructing a dense matrix.
    for (std::size_t i = 1; i + 1 < n; ++i) {
        const double left = x[i] - x[i - 1];
        const double right = x[i + 1] - x[i];
        const double width = left + right;
        if (!std::isfinite(width))
            return Result<Interpolator>{failure()};
        lower[i] = left / width;
        diagonal[i] = 2;
        upper[i] = right / width;
        rhs[i] = 6 * (((y[i + 1] - y[i]) / right - (y[i] - y[i - 1]) / left) / width);
    }
    if (boundary.first_derivative) {
        const double h = x[1] - x[0];
        diagonal[0] = 2;
        upper[0] = 1;
        rhs[0] = 6 * (((y[1] - y[0]) / h - *boundary.first_derivative) / h);
    }
    if (boundary.last_derivative) {
        const double h = x[n - 1] - x[n - 2];
        diagonal[n - 1] = 2;
        lower[n - 1] = 1;
        rhs[n - 1] = 6 * ((*boundary.last_derivative - (y[n - 1] - y[n - 2]) / h) / h);
    }
    for (std::size_t i = 1; i < n; ++i) {
        const double factor = lower[i] / diagonal[i - 1];
        diagonal[i] -= factor * upper[i - 1];
        rhs[i] -= factor * rhs[i - 1];
    }
    rhs[n - 1] /= diagonal[n - 1];
    for (std::size_t i = n - 1; i > 0; --i)
        rhs[i - 1] = (rhs[i - 1] - upper[i - 1] * rhs[i]) / diagonal[i - 1];
    if (!std::all_of(rhs.begin(), rhs.end(), [](double value) { return std::isfinite(value); }))
        return Result<Interpolator>{failure()};
    return Result<Interpolator>{Interpolator{{x.begin(), x.end()}, {y.begin(), y.end()}, std::move(rhs)}};
}

Result<double> Interpolator::evaluate(double x, OutOfRange policy) const {
    if (x_.size() < 2 || !std::isfinite(x) ||
        (policy != OutOfRange::reject && policy != OutOfRange::clamp && policy != OutOfRange::extrapolate))
        return Result<double>{Error{ErrorCode::invalid_input, "query", "Invalid table, query or range policy"}};
    if (x < x_.front() || x > x_.back()) {
        if (policy == OutOfRange::reject)
            return Result<double>{Error{ErrorCode::invalid_input, "query", "Query lies outside interpolation range"}};
        if (policy == OutOfRange::clamp)
            return Result<double>{x < x_.front() ? y_.front() : y_.back()};
    }
    const auto found = std::lower_bound(x_.begin(), x_.end(), x);
    if (found != x_.end() && *found == x)
        return Result<double>{y_[static_cast<std::size_t>(found - x_.begin())]};
    const auto pos = static_cast<std::size_t>(found - x_.begin());
    const auto i = pos == 0 ? 0 : std::min(pos - 1, x_.size() - 2);
    const double h = x_[i + 1] - x_[i];
    const double t = (x - x_[i]) / h;
    double value = std::lerp(y_[i], y_[i + 1], t);
    if (!second_.empty()) {
        const double a = 1 - t;
        value += ((a * a * a - a) * second_[i] + (t * t * t - t) * second_[i + 1]) * h * (h / 6);
    }
    if (!std::isfinite(value))
        return Result<double>{failure()};
    return Result<double>{value};
}
Result<std::vector<double>> Interpolator::evaluate(std::span<const double> x, OutOfRange policy) const {
    std::vector<double> values;
    values.reserve(x.size());
    for (double query : x) {
        auto value = evaluate(query, policy);
        if (!value)
            return Result<std::vector<double>>{value.error()};
        values.push_back(value.value());
    }
    return Result<std::vector<double>>{std::move(values)};
}
} // namespace qualpf::numerics
