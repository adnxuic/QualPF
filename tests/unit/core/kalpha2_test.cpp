#include "qualpf/domain/radiation.h"
#include "qualpf/preprocessing/kalpha2.h"
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
using namespace qualpf;
namespace {
Pattern sample() {
    Pattern p;
    p.id = {1};
    p.processing.kalpha2_subtracted = false;
    p.radiation = {RadiationKind::x_ray, {{radiation::default_wavelength, 1}}};
    for (int i = 0; i < 1000; ++i) {
        p.two_theta.emplace_back(10 + i * .02);
        p.observed.push_back(1 + 20 * std::exp(-.5 * std::pow((i - 500) / 10., 2)));
    }
    p.original_observed = p.observed;
    p.original_kalpha2_subtracted = false;
    return p;
}
} // namespace
TEST(Kalpha2, OwnsOutputPreservesInputAndRaw) {
    auto p = sample();
    const auto original = p.observed;
    auto result = preprocessing::strip_kalpha2(p);
    ASSERT_TRUE(result);
    EXPECT_NE(result.value(), original);
    EXPECT_EQ(p.observed, original);
    EXPECT_EQ(p.original_observed, original);
    EXPECT_EQ(p.processing.kalpha2_subtracted, false);
    p.observed[0] = 123;
    EXPECT_NE(result.value()[0], 123);
}
TEST(Kalpha2, RadiationContractIncludesCanonicalAndRoundedCu) {
    auto p = sample();
    for (double wave : {radiation::CU_KA1_WAVE, radiation::CU_KA_WAVE, 1.54056}) {
        p.radiation.lines[0].wavelength = WavelengthAngstrom{wave};
        EXPECT_TRUE(preprocessing::strip_kalpha2(p));
    }
    p.radiation.lines = {{radiation::default_wavelength, 1},
                         {WavelengthAngstrom{radiation::CU_KA2_WAVE}, .5}};
    EXPECT_TRUE(preprocessing::strip_kalpha2(p));
    p.radiation.lines[1].relative_weight = .4;
    auto r = preprocessing::strip_kalpha2(p);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().reason, "unsupported_kalpha2_radiation");
    p.radiation.lines.clear();
    EXPECT_FALSE(preprocessing::strip_kalpha2(p));
}
TEST(Kalpha2, RejectsFloatCollapsedGridAndOverflowWithoutMutation) {
    auto p = sample();
    p.two_theta[1] = TwoThetaDegrees{p.two_theta[0].value + 1e-12};
    auto r = preprocessing::strip_kalpha2(p);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().reason, "kalpha2_invalid_grid");
    p = sample();
    p.observed[0] = 1e300;
    r = preprocessing::strip_kalpha2(p);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().reason, "kalpha2_nonfinite_input");
    p = sample();
    p.observed[0] = -3e38;
    p.observed[1] = 3e38;
    auto original = p.observed;
    r = preprocessing::strip_kalpha2(p);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().reason, "kalpha2_nonfinite_result");
    EXPECT_EQ(p.observed, original);
}
TEST(Kalpha2, NoRawRequirementAndNoImplicitRawProvenance) {
    auto p = sample();
    p.original_observed.reset();
    p.original_kalpha2_subtracted.reset();
    EXPECT_TRUE(preprocessing::strip_kalpha2(p));
    EXPECT_FALSE(p.original_observed);
    p.processing.kalpha2_subtracted.reset();
    auto r = preprocessing::strip_kalpha2(p);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().reason, "kalpha2_state_unknown");
}
