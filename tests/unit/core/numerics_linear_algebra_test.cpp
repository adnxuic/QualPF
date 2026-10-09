#include "qualpf/numerics/linear_algebra.h"
#include "qualpf/numerics/polynomial.h"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>

using namespace qualpf;
using namespace qualpf::numerics;

namespace {
void expect_near(std::span<const double> actual, std::span<const double> expected, double tolerance = 1e-12) {
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t i = 0; i < actual.size(); ++i)
        EXPECT_NEAR(actual[i], expected[i], tolerance) << i;
}
Matrix transposed(const Matrix &a) {
    Matrix out{a.columns, a.rows, std::vector<double>(a.values.size())};
    for (std::size_t i = 0; i < a.rows; ++i)
        for (std::size_t j = 0; j < a.columns; ++j)
            out.values[j * a.rows + i] = a.values[i * a.columns + j];
    return out;
}
} // namespace

TEST(NumericsLinearAlgebra, LuPivotsAndReturnsOriginalUnitResidual) {
    const Matrix a{2, 2, {0, 2, 1, 3}};
    const auto result = solve_linear(a.view(), std::array{4.0, 7.0});
    ASSERT_TRUE(result);
    expect_near(result.value().x, std::array{1.0, 2.0});
    EXPECT_EQ(result.value().rank, 2u);
    EXPECT_NEAR(result.value().residual_norm, 0, 1e-12);
    EXPECT_NEAR(determinant(a.view()).value(), -2, 1e-12);
    auto inv = inverse(a.view());
    ASSERT_TRUE(inv);
    expect_near(multiply(a.view(), inv.value().view()).value().values, std::array{1.0, 0.0, 0.0, 1.0});
    EXPECT_EQ(a.values, (std::vector<double>{0, 2, 1, 3}));
}
TEST(NumericsLinearAlgebra, QrReconstructsPermutedTallAndWideMatrices) {
    for (const Matrix a : {Matrix{3, 2, {1, 10, 1, 20, 1, 40}}, Matrix{2, 3, {1, 10, 2, 2, 5, 1}}}) {
        auto result = qr(a.view());
        ASSERT_TRUE(result);
        const auto &f = result.value();
        auto product = multiply(f.q.view(), f.r.view());
        ASSERT_TRUE(product);
        for (std::size_t i = 0; i < a.rows; ++i)
            for (std::size_t j = 0; j < a.columns; ++j)
                EXPECT_NEAR(product.value().values[i * a.columns + j], a.values[i * a.columns + f.permutation[j]],
                            1e-12);
        const auto qt = transposed(f.q);
        const auto qtq = multiply(qt.view(), f.q.view());
        ASSERT_TRUE(qtq);
        for (std::size_t i = 0; i < qtq.value().rows; ++i)
            for (std::size_t j = 0; j < qtq.value().columns; ++j)
                EXPECT_NEAR(qtq.value().values[i * qtq.value().columns + j], i == j ? 1 : 0, 1e-12);
    }
}
TEST(NumericsLinearAlgebra, SvdReconstructsRectangularMatricesWithoutDependingOnSigns) {
    for (const Matrix a : {Matrix{3, 2, {1, 0, 0, 2, 1, 1}}, Matrix{2, 3, {1, 0, 1, 0, 2, 1}}}) {
        auto result = svd(a.view());
        ASSERT_TRUE(result);
        auto us = result.value().u;
        const auto &s = result.value().singular_values;
        EXPECT_GE(s[0], s[1]);
        EXPECT_GE(s[1], 0);
        for (std::size_t i = 0; i < us.rows; ++i)
            for (std::size_t j = 0; j < us.columns; ++j)
                us.values[i * us.columns + j] *= s[j];
        const auto vt = transposed(result.value().v);
        expect_near(multiply(us.view(), vt.view()).value().values, a.values);
    }
}
TEST(NumericsLinearAlgebra, LeastSquaresMatchesAnalyticProjectionAndWeighting) {
    const Matrix a{3, 2, {1, 0, 1, 1, 1, 2}};
    const std::array b{1.0, 2.0, 2.0};
    for (auto method : {LeastSquaresMethod::pivoted_qr, LeastSquaresMethod::svd}) {
        auto result = least_squares(a.view(), b, {method});
        ASSERT_TRUE(result);
        expect_near(result.value().x, std::array{7.0 / 6, 0.5});
        EXPECT_NEAR(result.value().residual_norm, std::sqrt(1.0 / 6), 1e-12);
    }
    const Matrix constant{2, 1, {1, 1}};
    auto weighted = least_squares(constant.view(), std::array{1.0, 3.0}, {}, std::array{1.0, 2.0});
    ASSERT_TRUE(weighted);
    expect_near(weighted.value().x, std::array{1.4});
    EXPECT_NEAR(weighted.value().objective_norm, std::sqrt(0.8), 1e-12);
    EXPECT_NEAR(weighted.value().residual_norm, std::sqrt(2.72), 1e-12);
}
TEST(NumericsLinearAlgebra, SvdGivesMinimumNormAndRankDeficiencyIsExplicit) {
    const Matrix a{2, 2, {1, 1, 2, 2}};
    EXPECT_EQ(solve_linear(a.view(), std::array{2.0, 4.0}).error().code, ErrorCode::rank_deficient);
    EXPECT_FALSE(inverse(a.view()));
    EXPECT_FALSE(least_squares(a.view(), std::array{2.0, 4.0}));
    EXPECT_FALSE(least_squares(a.view(), std::array{2.0, 4.0}, {LeastSquaresMethod::svd}));
    auto result = least_squares(a.view(), std::array{2.0, 4.0}, {LeastSquaresMethod::svd, {}, true});
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().rank, 1u);
    expect_near(result.value().x, std::array{1.0, 1.0});
    const Matrix wide{1, 2, {1, 1}};
    auto minimum = least_squares(wide.view(), std::array{2.0}, {LeastSquaresMethod::svd});
    ASSERT_TRUE(minimum);
    expect_near(minimum.value().x, std::array{1.0, 1.0});
    EXPECT_FALSE(least_squares(wide.view(), std::array{2.0}));
}
TEST(NumericsLinearAlgebra, RankToleranceIsScaleInvariantAndValidated) {
    for (double scale : {1e-200, 1.0, 1e200}) {
        const Matrix a{2, 2, {scale, 0, 0, scale * 1e-8}};
        EXPECT_EQ(svd(a.view(), {1e-6}).value().rank, 1u);
        EXPECT_EQ(qr(a.view(), {1e-6}).value().rank, 1u);
        EXPECT_FALSE(solve_linear(a.view(), std::array{scale, scale * 1e-8}, {1e-6}));
        auto result = solve_linear(a.view(), std::array{scale, scale * 1e-8}, {1e-10});
        ASSERT_TRUE(result);
        expect_near(result.value().x, std::array{1.0, 1.0});
    }
    const Matrix identity{1, 1, {1}};
    EXPECT_FALSE(qr(identity.view(), {-1.0}));
    EXPECT_FALSE(svd(identity.view(), {1.0}));
}
TEST(NumericsLinearAlgebra, NearCollinearLeastSquaresDoesNotSquareConditionNumber) {
    const Matrix a{3, 2, {1, 1, 1, 1 + 1e-7, 1, 1 - 1e-7}};
    const std::array b{3.0, 3 + 2e-7, 3 - 2e-7};
    for (auto method : {LeastSquaresMethod::pivoted_qr, LeastSquaresMethod::svd}) {
        auto fit = least_squares(a.view(), b, {method});
        ASSERT_TRUE(fit);
        expect_near(fit.value().x, std::array{1.0, 2.0}, 1e-8);
        EXPECT_LT(fit.value().residual_norm, 1e-14);
    }
    const Matrix zero{2, 2, {0, 0, 0, 0}};
    auto fit = least_squares(zero.view(), std::array{3.0, 4.0}, {LeastSquaresMethod::svd, {}, true});
    ASSERT_TRUE(fit);
    EXPECT_EQ(fit.value().rank, 0u);
    expect_near(fit.value().x, std::array{0.0, 0.0});
    EXPECT_NEAR(fit.value().residual_norm, 5, 1e-14);
}
TEST(NumericsLinearAlgebra, RejectsMalformedNonfiniteAndOverflowInputsBeforeMapping) {
    EXPECT_FALSE(qr({0, 1, {}}));
    EXPECT_FALSE(svd({std::numeric_limits<std::size_t>::max(), 2, {}}));
    EXPECT_FALSE(inverse({2, 2, std::array{1.0, 2.0}}));
    const Matrix a{2, 2, {1, 0, 0, 1}};
    EXPECT_FALSE(solve_linear(a.view(), std::array{1.0}));
    EXPECT_FALSE(least_squares(a.view(), std::array{1.0, 2.0}, {}, std::array{0.0, 1.0}));
    EXPECT_FALSE(least_squares(a.view(), std::array{1.0, 2.0}, {}, std::array{1.0}));
    const Matrix bad{1, 1, {std::numeric_limits<double>::quiet_NaN()}};
    EXPECT_FALSE(svd(bad.view()));
    const Matrix large{1, 1, {1e308}};
    EXPECT_FALSE(multiply(large.view(), large.view()));
}
TEST(NumericsPolynomial, FitsScaledCoordinatesAndEvaluatesInOriginalUnits) {
    const std::array x{999998.0, 999999.0, 1000000.0, 1000001.0, 1000002.0};
    const std::array y{9.0, 2.0, 1.0, 6.0, 17.0}; // 1 + 2*t + 3*t^2
    auto fit = fit_polynomial(x, y, 2, {LeastSquaresMethod::svd});
    ASSERT_TRUE(fit);
    EXPECT_DOUBLE_EQ(fit.value().center, 1000000);
    EXPECT_NEAR(fit.value().scale, std::sqrt(2.5), 1e-14);
    expect_near(fit.value().coefficients, std::array{1.0, 2 * std::sqrt(2.5), 7.5});
    EXPECT_NEAR(evaluate_polynomial(fit.value(), 1000000.5).value(), 2.75, 1e-12);
    EXPECT_LT(fit.value().residual_norm, 1e-12);
}
TEST(NumericsPolynomial, RejectsDegenerateFitsAndSupportsConstantFit) {
    EXPECT_FALSE(fit_polynomial({}, {}, 0));
    EXPECT_FALSE(fit_polynomial(std::array{1.0, 1.0}, std::array{1.0, 2.0}, 1));
    EXPECT_FALSE(fit_polynomial(std::array{1.0, 2.0}, std::array{1.0, 2.0}, 2));
    auto constant = fit_polynomial(std::array{5.0}, std::array{7.0}, 0);
    ASSERT_TRUE(constant);
    EXPECT_DOUBLE_EQ(evaluate_polynomial(constant.value(), 10).value(), 7);
    EXPECT_FALSE(evaluate_polynomial(PolynomialFit{{1}, 0, 0, 1, 0, 0}, 1));
}
