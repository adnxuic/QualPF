#include "regression_case.h"

#include <gtest/gtest.h>
#include <type_traits>

static_assert(!std::is_default_constructible_v<qualpf::testing::FieldTolerance>);

TEST(RegressionContract, AwaitingReferenceHasNoExpectedAnswerOrImplicitTolerance)
{
    qualpf::testing::ScoreRegressionCase fixture;
    EXPECT_EQ(fixture.evidence, qualpf::testing::EvidenceState::awaiting_reference);
    EXPECT_FALSE(fixture.expected.has_value());
    EXPECT_TRUE(fixture.tolerances.empty());
}
