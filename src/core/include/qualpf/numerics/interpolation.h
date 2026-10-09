#pragma once

#include "qualpf/numerics/basic.h"

#include <optional>

namespace qualpf::numerics {

// nullopt means natural (second derivative zero); a value specifies the first
// derivative. In particular, 0.0 is a clamped, NOT a natural endpoint.
struct SplineBoundary {
    std::optional<double> first_derivative;
    std::optional<double> last_derivative;
};

// Validated owning table. No cached mutable cursor; concurrent const queries
// have no shared mutable state. Input knots must increase strictly.
class Interpolator {
  public:
    Result<double> evaluate(double x, OutOfRange policy = OutOfRange::reject) const;
    Result<std::vector<double>> evaluate(std::span<const double> x, OutOfRange policy = OutOfRange::reject) const;

  private:
    Interpolator(std::vector<double> x, std::vector<double> y, std::vector<double> second);
    std::vector<double> x_;
    std::vector<double> y_;
    std::vector<double> second_; // empty for piecewise linear interpolation
    friend Result<Interpolator> linear_interpolator(std::span<const double>, std::span<const double>);
    friend Result<Interpolator> cubic_spline(std::span<const double>, std::span<const double>, SplineBoundary);
};

Result<Interpolator> linear_interpolator(std::span<const double> x, std::span<const double> y);
Result<Interpolator> cubic_spline(std::span<const double> x, std::span<const double> y, SplineBoundary boundary = {});

} // namespace qualpf::numerics
