#pragma once

#include "qualpf/numerics/linear_algebra.h"

namespace qualpf::numerics {

struct PolynomialFit {
    // Ascending powers of z = (x - center) / scale, NOT powers of raw x.
    std::vector<double> coefficients;
    double center;
    double scale; // sample standard deviation, or 1 for a constant fit
    std::size_t rank;
    double residual_norm;
    double objective_norm;
};
// General polynomial fitting only; no peak/background model is defined here.
Result<PolynomialFit> fit_polynomial(std::span<const double> x, std::span<const double> y, std::size_t degree,
                                     LeastSquaresOptions options = {}, std::span<const double> sigma = {});
Result<double> evaluate_polynomial(const PolynomialFit &polynomial, double x);

} // namespace qualpf::numerics
