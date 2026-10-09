#include "qualpf/domain/validation.h"
#include "qualpf/migration/backends.h"

#include <gtest/gtest.h>
#include <limits>
#include <type_traits>

using namespace qualpf;

static_assert(!std::is_convertible_v<TwoThetaDegrees, DSpacingAngstrom>);
static_assert(!std::is_convertible_v<WavelengthAngstrom, DSpacingAngstrom>);
static_assert(!std::is_default_constructible_v<Result<ScoreValues>>);

TEST(CoreContracts, ValidationRejectsBadGridWithoutSortingOrRepairing)
{
    Pattern pattern;
    pattern.id = {1};
    pattern.two_theta = {TwoThetaDegrees{20}, TwoThetaDegrees{10}};
    pattern.observed = {1, 2};
    EXPECT_FALSE(validate(pattern).empty());
    EXPECT_EQ(pattern.two_theta.front().value, 20);
    pattern.two_theta[1] = TwoThetaDegrees{30};
    pattern.observed[1] = std::numeric_limits<double>::quiet_NaN();
    EXPECT_FALSE(validate(pattern).empty());
}

TEST(CoreContracts, MissingIntensityAndMissingProcessingHistoryAreNotZero)
{
    Peak peak;
    Pattern pattern;
    EXPECT_FALSE(peak.original_intensity.has_value());
    EXPECT_FALSE(pattern.processing.kalpha2_subtracted.has_value());
    Result<ScoreValues> error{Error{ErrorCode::unsupported, "scoring", "No verified backend"}};
    EXPECT_FALSE(error.has_value());
}

TEST(CoreContracts, BackgroundWithUnknownAlignmentIsPreservedAsUnknown)
{
    Pattern pattern;
    pattern.id = {1};
    pattern.two_theta = {TwoThetaDegrees{10}, TwoThetaDegrees{20}};
    pattern.observed = {8, 9};
    pattern.background = SampledSignal{{2}, std::nullopt};
    EXPECT_TRUE(validate(pattern).empty());
    EXPECT_FALSE(pattern.background->window.has_value());
    pattern.background->window = SampleWindow{2};
    EXPECT_FALSE(validate(pattern).empty());
}

TEST(CoreContracts, ScoringContractRequiresTheExplicitPatternRange)
{
    Pattern pattern;
    pattern.id = {1};
    pattern.two_theta = {TwoThetaDegrees{10}, TwoThetaDegrees{20}};
    pattern.observed = {8, 9};
    PeakSet peaks;
    peaks.dataset = {1};
    Peak peak;
    peak.id = {1};
    peak.two_theta = TwoThetaDegrees{10};
    peaks.peaks.push_back(peak);
    ReferenceCard card;
    card.database_id = "synthetic";
    card.card_id = "one";
    Reflection reflection;
    reflection.id = {1};
    reflection.two_theta = TwoThetaDegrees{10};
    card.reflections.push_back(reflection);
    SearchParameters parameters;
    parameters.delta_two_theta = AngularWidthDegrees{0.1};
    ScoreInput input{pattern, peaks, card, parameters};
    EXPECT_FALSE(validate(input).empty());
    pattern.calculation_range = AngularRange{TwoThetaDegrees{10}, TwoThetaDegrees{20}};
    EXPECT_TRUE(validate(input).empty());
}
