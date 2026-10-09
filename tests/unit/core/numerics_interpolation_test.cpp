#include "qualpf/numerics/interpolation.h"

#include <gtest/gtest.h>

#include <array>
#include <limits>

using namespace qualpf::numerics;

TEST(NumericsInterpolation, LinearTableOwnsInputAndDefinesRangePolicy) {
    std::vector<double> x{0, 1, 3}, y{1, 3, 7};
    auto table = linear_interpolator(x, y);
    ASSERT_TRUE(table);
    x[0] = 99;
    y[0] = 99;
    EXPECT_DOUBLE_EQ(table.value().evaluate(0).value(), 1);
    EXPECT_DOUBLE_EQ(table.value().evaluate(2).value(), 5);
    EXPECT_DOUBLE_EQ(table.value().evaluate(3).value(), 7);
    EXPECT_FALSE(table.value().evaluate(-1));
    EXPECT_DOUBLE_EQ(table.value().evaluate(-1, OutOfRange::clamp).value(), 1);
    EXPECT_DOUBLE_EQ(table.value().evaluate(4, OutOfRange::extrapolate).value(), 9);
    EXPECT_EQ(table.value().evaluate(std::array{3.0, 0.0, 2.0}).value(), (std::vector<double>{7, 1, 5}));
    EXPECT_FALSE(table.value().evaluate(std::array{0.0, 4.0}));
}
TEST(NumericsInterpolation, NaturalAndClampedEndpointsAreDifferent) {
    const std::array x{0.0, 1.0}, y{0.0, 1.0};
    auto natural = cubic_spline(x, y);
    auto clamped = cubic_spline(x, y, {0.0, 0.0});
    ASSERT_TRUE(natural);
    ASSERT_TRUE(clamped);
    EXPECT_NEAR(natural.value().evaluate(0.25).value(), 0.25, 1e-14);
    // Hermite cubic 3*x^2 - 2*x^3, zero slopes at both endpoints.
    EXPECT_NEAR(clamped.value().evaluate(0.25).value(), 0.15625, 1e-14);
    EXPECT_DOUBLE_EQ(clamped.value().evaluate(0).value(), 0);
    EXPECT_DOUBLE_EQ(clamped.value().evaluate(1).value(), 1);
}
TEST(NumericsInterpolation, ClampedSplineReproducesCubicOnIrregularKnots) {
    const std::array x{0.0, 0.5, 2.0, 3.0}, y{0.0, 0.125, 8.0, 27.0};
    auto table = cubic_spline(x, y, {0.0, 27.0});
    ASSERT_TRUE(table);
    for (double query : {0.25, 1.0, 1.5, 2.5, 3.25})
        EXPECT_NEAR(table.value().evaluate(query, OutOfRange::extrapolate).value(), query * query * query, 2e-13);
}
TEST(NumericsInterpolation, NaturalSplineMatchesIndependentThreeKnotSolution) {
    // Uniform knots, endpoint second derivatives zero: M_middle = -3.
    auto table = cubic_spline(std::array{0.0, 1.0, 2.0}, std::array{0.0, 1.0, 0.0});
    ASSERT_TRUE(table);
    EXPECT_NEAR(table.value().evaluate(0.5).value(), 0.6875, 1e-14);
    EXPECT_NEAR(table.value().evaluate(1.5).value(), 0.6875, 1e-14);
}
TEST(NumericsInterpolation, InvalidTablesAndQueriesReturnErrors) {
    EXPECT_FALSE(linear_interpolator({}, {}));
    EXPECT_FALSE(cubic_spline(std::array{1.0}, std::array{1.0}));
    EXPECT_FALSE(cubic_spline(std::array{0.0, 0.0}, std::array{1.0, 2.0}));
    EXPECT_FALSE(linear_interpolator(std::array{1.0, 0.0}, std::array{1.0, 2.0}));
    EXPECT_FALSE(linear_interpolator(std::array{0.0, 1.0}, std::array{1.0}));
    EXPECT_FALSE(
        cubic_spline(std::array{0.0, 1.0}, std::array{1.0, 2.0}, {std::numeric_limits<double>::infinity(), 0.0}));
    auto table = linear_interpolator(std::array{0.0, 1.0}, std::array{1.0, 2.0});
    ASSERT_TRUE(table);
    EXPECT_FALSE(table.value().evaluate(std::numeric_limits<double>::quiet_NaN()));
}
