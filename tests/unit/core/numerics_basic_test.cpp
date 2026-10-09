#include "qualpf/numerics/basic.h"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>

using namespace qualpf;
using namespace qualpf::numerics;

TEST(NumericsBasic, BraggConversionsHaveExplicitUnitsAndRoundTrip) {
    const WavelengthAngstrom wave{1.54056};
    for (double angle : {0.001, 20.0, 60.0, 120.0, 179.0, 180.0}) {
        const auto d = to_d_spacing(TwoThetaDegrees{angle}, wave);
        ASSERT_TRUE(d);
        const auto converted = to_two_theta(d.value(), wave);
        ASSERT_TRUE(converted);
        EXPECT_NEAR(converted.value().value, angle, 2e-10);
    }
    EXPECT_NEAR(to_d_spacing(TwoThetaDegrees{60}, wave).value().value, wave.value, 1e-14);
    EXPECT_NEAR(degrees(radians(31)), 31, 1e-14);
    EXPECT_FALSE(to_d_spacing(TwoThetaDegrees{0}, wave));
    EXPECT_FALSE(to_d_spacing(TwoThetaDegrees{181}, wave));
    EXPECT_FALSE(to_two_theta(DSpacingAngstrom{0.1}, wave));
    EXPECT_FALSE(to_two_theta(DSpacingAngstrom{1}, WavelengthAngstrom{-1}));
    EXPECT_FALSE(to_two_theta(DSpacingAngstrom{std::numeric_limits<double>::infinity()}, wave));
}
TEST(NumericsBasic, CheckedConversionsRejectOverflowAndNonfinite) {
    EXPECT_EQ(nearest_integer(2.5).value(), 3);
    EXPECT_EQ(nearest_integer(-2.5).value(), -3);
    EXPECT_EQ(nearest_integer(-2147483648.0).value(), std::numeric_limits<std::int32_t>::min());
    EXPECT_FALSE(nearest_integer(2147483647.5));
    EXPECT_FALSE(nearest_integer(std::numeric_limits<double>::quiet_NaN()));
    EXPECT_EQ(to_float32(1.25).value(), 1.25f);
    EXPECT_FALSE(to_float32(1e100));
    EXPECT_FALSE(to_float32(std::numeric_limits<double>::infinity()));
}
TEST(NumericsBasic, ReductionsHandleCancellationAndLargeNorms) {
    const std::array<double, 3> values{1e16, 1, -1e16};
    EXPECT_DOUBLE_EQ(sum(values).value(), 1);
    EXPECT_DOUBLE_EQ(sum({}).value(), 0);
    EXPECT_DOUBLE_EQ(dot(std::array{1.0, 2.0}, std::array{3.0, 4.0}).value(), 11);
    EXPECT_FALSE(dot(std::array{1.0}, std::array{2.0, 3.0}));
    EXPECT_NEAR(norm(std::array{3e200, 4e200}).value() / 1e200, 5, 1e-14);
    EXPECT_EQ(sum(std::array{1e308, 1e308}).error().code, ErrorCode::numerical_failure);
    EXPECT_FALSE(norm(std::array{std::numeric_limits<double>::quiet_NaN()}));
}
TEST(NumericsBasic, StatisticsUseSampleVarianceAndPreserveSmallSpread) {
    const std::array<double, 4> data{1e12 + 1, 1e12 + 2, 1e12 + 3, 1e12 + 4};
    const auto result = statistics(data);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().count, 4u);
    EXPECT_DOUBLE_EQ(result.value().mean, 1e12 + 2.5);
    EXPECT_NEAR(result.value().variance, 5.0 / 3, 1e-14);
    EXPECT_NEAR(statistics(data, Variance::population).value().variance, 1.25, 1e-14);
    EXPECT_DOUBLE_EQ(statistics(std::array{7.0, 7.0}).value().variance, 0);
    EXPECT_FALSE(statistics({}));
    EXPECT_FALSE(statistics(std::array{1.0}));
    EXPECT_DOUBLE_EQ(statistics(std::array{1.0}, Variance::population).value().variance, 0);
    EXPECT_FALSE(statistics(std::array{1.0, std::numeric_limits<double>::infinity()}));
}
TEST(NumericsBasic, PermutationsAreStableAndDoNotChangeInput) {
    const std::array keys{3.0, 1.0, 1.0, 2.0};
    auto order = sort_indices(keys);
    ASSERT_TRUE(order);
    EXPECT_EQ(order.value(), (std::vector<std::size_t>{1, 2, 3, 0}));
    EXPECT_EQ(keys, (std::array{3.0, 1.0, 1.0, 2.0}));
    EXPECT_TRUE(sort_indices({}).value().empty());
    EXPECT_FALSE(sort_indices(std::array{std::numeric_limits<double>::quiet_NaN()}));
}
TEST(NumericsBasic, LocateHasDefinedEndpointsDirectionAndErrors) {
    const std::array grid{0.0, 1.0, 4.0};
    EXPECT_EQ(locate_interval(grid, 0).value(), 0u);
    EXPECT_EQ(locate_interval(grid, 1).value(), 1u);
    EXPECT_EQ(locate_interval(grid, 4).value(), 1u);
    EXPECT_FALSE(locate_interval(grid, -1));
    EXPECT_EQ(locate_interval(grid, -1, OutOfRange::extrapolate).value(), 0u);
    EXPECT_EQ(locate_interval(std::array{4.0, 1.0, 0.0}, 1).value(), 1u);
    EXPECT_FALSE(locate_interval(std::array{1.0, 1.0}, 1));
    EXPECT_FALSE(locate_interval(std::array{1.0, 3.0, 2.0}, 2));
    EXPECT_FALSE(locate_interval(std::array{1.0}, 1));
}
TEST(NumericsBasic, NearestPointClampsAndUsesHigherIndexForHalfwayTies) {
    const std::array grid{0.0, 2.0, 5.0};
    EXPECT_EQ(nearest_index(grid, -1).value(), 0u);
    EXPECT_EQ(nearest_index(grid, 1).value(), 1u);
    EXPECT_EQ(nearest_index(grid, 3.49).value(), 1u);
    EXPECT_EQ(nearest_index(grid, 3.5).value(), 2u);
    EXPECT_EQ(nearest_index(grid, 99).value(), 2u);
    EXPECT_EQ(nearest_index(std::array{5.0}, 0).value(), 0u);
    EXPECT_FALSE(nearest_index({}, 1));
    EXPECT_FALSE(nearest_index(std::array{1.0, 1.0}, 1));
}
TEST(NumericsBasic, GridAndIntegrationDefineDegenerateCases) {
    EXPECT_EQ(linspace(1, 3, 3).value(), (std::vector<double>{1, 2, 3}));
    EXPECT_EQ(linspace(3, 1, 3).value(), (std::vector<double>{3, 2, 1}));
    EXPECT_DOUBLE_EQ(linspace(-1e308, 1e308, 3).value()[1], 0);
    EXPECT_EQ(linspace(2, 2, 1).value(), (std::vector<double>{2}));
    EXPECT_FALSE(linspace(1, 2, 1));
    EXPECT_FALSE(linspace(1, 2, 0));
    EXPECT_NEAR(trapezoid(std::array{0.0, 1.0, 3.0}, std::array{1.0, 3.0, 7.0}).value(), 12, 1e-14);
    EXPECT_DOUBLE_EQ(trapezoid(std::array{1.0}, std::array{5.0}).value(), 0);
    EXPECT_DOUBLE_EQ(trapezoid(std::array{1.0}, std::array{5.0}, SinglePointIntegral::legacy_sample_value).value(), 5);
    EXPECT_FALSE(trapezoid({}, {}));
    EXPECT_FALSE(trapezoid(std::array{1.0, 0.0}, std::array{1.0, 2.0}));
}
