#include "qualpf/numerics/polynomial.h"
#include "qualpf/numerics/basic.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace qualpf::numerics {
Result<PolynomialFit> fit_polynomial(std::span<const double> x, std::span<const double> y, std::size_t degree,
                                     LeastSquaresOptions options, std::span<const double> sigma) {
    if (x.empty() || x.size() != y.size() || degree >= x.size() ||
        degree + 1 > std::numeric_limits<std::size_t>::max() / x.size())
        return Result<PolynomialFit>{Error{ErrorCode::invalid_input, "polynomial",
                                           "Require paired samples and degree < sample count within size range"}};
    auto stats = statistics(x, x.size() == 1 ? Variance::population : Variance::sample);
    if (!stats)
        return Result<PolynomialFit>{stats.error()};
    const double center = stats.value().mean;
    const double scale = degree == 0 ? 1 : std::sqrt(stats.value().variance);
    if (scale <= 0)
        return Result<PolynomialFit>{
            Error{ErrorCode::rank_deficient, "x", "Nonconstant polynomial requires nonzero coordinate spread"}};
    Matrix design{x.size(), degree + 1, std::vector<double>(x.size() * (degree + 1), 1)};
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double z = (x[i] - center) / scale;
        for (std::size_t j = 1; j <= degree; ++j)
            design.values[i * (degree + 1) + j] = design.values[i * (degree + 1) + j - 1] * z;
    }
    if (!std::all_of(design.values.begin(), design.values.end(), [](double v) { return std::isfinite(v); }))
        return Result<PolynomialFit>{
            Error{ErrorCode::numerical_failure, "design", "Polynomial basis exceeded finite range"}};
    auto fit = least_squares(design.view(), y, options, sigma);
    if (!fit)
        return Result<PolynomialFit>{fit.error()};
    return Result<PolynomialFit>{PolynomialFit{std::move(fit.value().x), center, scale, fit.value().rank,
                                               fit.value().residual_norm, fit.value().objective_norm}};
}
Result<double> evaluate_polynomial(const PolynomialFit &polynomial, double x) {
    if (polynomial.coefficients.empty() || !std::isfinite(x) || !std::isfinite(polynomial.center) ||
        !std::isfinite(polynomial.scale) || polynomial.scale <= 0 ||
        !std::all_of(polynomial.coefficients.begin(), polynomial.coefficients.end(),
                     [](double v) { return std::isfinite(v); }))
        return Result<double>{Error{ErrorCode::invalid_input, "polynomial",
                                    "Require finite coefficients, center, query and positive scale"}};
    const double z = (x - polynomial.center) / polynomial.scale;
    double value = polynomial.coefficients.back();
    for (std::size_t i = polynomial.coefficients.size() - 1; i > 0; --i)
        value = value * z + polynomial.coefficients[i - 1];
    if (!std::isfinite(value))
        return Result<double>{
            Error{ErrorCode::numerical_failure, "polynomial", "Polynomial evaluation exceeded finite range"}};
    return Result<double>{value};
}
} // namespace qualpf::numerics
