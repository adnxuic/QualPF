#include "classic_fixture.h"
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <stop_token>

using namespace qualpf;
using namespace qualpf::search;
using qualpf::testing::ClassicFixture;

TEST(ClassicScore, PerfectMatchAndBackendHaveAnalyticUnitScore) {
    ClassicFixture f;
    auto result = score_classic(f.input(), f.context);
    ASSERT_TRUE(result);
    const auto &r = result.value();
    EXPECT_EQ(r.disposition, ScoreDisposition::scored);
    EXPECT_DOUBLE_EQ(r.score.total, 1);
    EXPECT_DOUBLE_EQ(r.score.position, 1);
    EXPECT_DOUBLE_EQ(r.score.intensity, 1);
    EXPECT_DOUBLE_EQ(r.score.scale, 10);
    EXPECT_EQ(r.associated_reflections, 3);
    EXPECT_EQ(r.associated_peaks, 3);
    EXPECT_EQ(r.merged_intensities, (std::vector<double>{10, 20, 10}));
    EXPECT_EQ(r.scaled_intensities, (std::vector<double>{100, 200, 100}));
    ClassicScorer backend;
    auto s = backend.score(f.input(), f.context);
    ASSERT_TRUE(s);
    EXPECT_EQ(s.value().total, r.score.total);
}

TEST(ClassicScore, OverlappingReflectionsMergeBeforeScaleFit) {
    ClassicFixture f;
    f.set_card({{10, 4}, {10, 6}, {20, 20}, {30, 10}});
    auto result = score_classic(f.input(), f.context);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().associated_reflections, 4);
    EXPECT_EQ(result.value().associated_peaks, 3);
    EXPECT_EQ(result.value().experimental_indices[0], 0);
    EXPECT_EQ(result.value().experimental_indices[1], 0);
    EXPECT_EQ(result.value().score.total, 1);
    EXPECT_EQ(result.value().score.scale, 10);
}

TEST(ClassicScore, ScaleIsThroughOriginLeastSquaresWithIntensityMismatch) {
    ClassicFixture f;
    f.set_peaks({{10, 2}, {20, 4}});
    f.set_card({{10, 1}, {20, 1}});
    auto result = score_classic(f.input(), f.context);
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(result.value().score.scale, 3);
    // Mean relative discrepancy = (1/3 + 1/4)/2 = 7/24.
    EXPECT_NEAR(result.value().score.intensity, 17.0 / 24.0, 1e-7);
    EXPECT_NEAR(result.value().score.total, std::sqrt(65.0 / 72.0), 1e-7);
}

TEST(ClassicScore, WindowIsStrictAndRangeIsInclusiveBeforeRealConversion) {
    ClassicFixture f;
    f.parameters.delta_two_theta = AngularWidthDegrees{0.125};
    f.set_peaks({{10, 100}});
    f.set_card({{10.125, 10}});
    auto boundary = score_classic(f.input(), f.context);
    ASSERT_TRUE(boundary);
    EXPECT_EQ(boundary.value().disposition, ScoreDisposition::no_associations);
    f.parameters.delta_two_theta.value = std::nextafter(0.125, 1.0);
    auto inside = score_classic(f.input(), f.context);
    ASSERT_TRUE(inside);
    EXPECT_EQ(inside.value().associated_reflections, 1);
    f.pattern.calculation_range = AngularRange{TwoThetaDegrees{20}, TwoThetaDegrees{30}};
    f.parameters.delta_two_theta.value = 0.25;
    f.set_card({{19.75, 1}, {30.25, 1}, {std::nextafter(30.25, 40.0), 1}});
    auto range = score_classic(f.input(), f.context);
    ASSERT_TRUE(range);
    EXPECT_EQ(range.value().reflection_indices, (std::vector<std::size_t>{0, 1}));
}

TEST(ClassicScore, MixedPrecisionCanMoveAssociationAcrossABoundary) {
    ClassicFixture f;
    f.set_peaks({{10, 100}});
    f.set_card({{10.125 - 1e-8, 10}}); // Narrows to 10.125f before subtraction.
    f.parameters.delta_two_theta.value = 0.125;
    auto result = score_classic(f.input(), f.context);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().associated_reflections, 0);
}

TEST(ClassicScore, NearestTiesUseHigherIndexAndDuplicatesFollowClocateEndpoints) {
    ClassicFixture f;
    f.set_peaks({{10, 100}, {12, 100}});
    f.set_card({{11, 10}});
    f.parameters.delta_two_theta.value = 2;
    auto result = score_classic(f.input(), f.context);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().experimental_indices[0], 1);
    f.set_peaks({{10, 100}, {10, 100}, {20, 100}, {20, 100}});
    f.set_card({{10, 10}, {20, 10}});
    result = score_classic(f.input(), f.context);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().experimental_indices[0], 0);
    EXPECT_EQ(result.value().experimental_indices[1], 3);
}

TEST(ClassicScore, MissingCardPeakUsesNearestObservedCountAndStrictIntensityComparison) {
    ClassicFixture f;
    f.set_card({{10, 10}, {20, 20}, {30, 10}, {40, 10}});
    f.pattern.observed[4] = 100;
    auto equal = score_classic(f.input(), f.context);
    ASSERT_TRUE(equal);
    EXPECT_EQ(equal.value().unassociated_below_observed, 0);
    EXPECT_NEAR(equal.value().score.total, std::pow(0.6, 0.25), 1e-7);
    f.pattern.observed[4] = 101;
    auto below = score_classic(f.input(), f.context);
    ASSERT_TRUE(below);
    EXPECT_EQ(below.value().unassociated_below_observed, 1);
    EXPECT_NEAR(below.value().score.total, std::pow(0.8, 0.25), 1e-7);
    EXPECT_EQ(below.value().total_card_sum, 500);
}

TEST(ClassicScore, MissingSamplePeaksAndPhaseCountKeepOriginalFormula) {
    ClassicFixture f;
    f.set_peaks({{10, 100}, {20, 100}, {30, 100}, {40, 100}});
    f.set_card({{10, 10}});
    f.parameters.weight_phases = 1;
    auto result = score_classic(f.input(), f.context);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().expected_peaks_per_phase, 4);
    EXPECT_EQ(result.value().database_fraction, 0.5);
    EXPECT_EQ(result.value().sample_fraction, 0.25);
    EXPECT_NEAR(result.value().score.total, std::sqrt(std::sqrt(0.5) * 0.5), 1e-7);
    f.parameters.weight_intensity = 0;
    auto no_intensity_weight = score_classic(f.input(), f.context);
    ASSERT_TRUE(no_intensity_weight);
    EXPECT_EQ(no_intensity_weight.value().score.total, result.value().score.total);
    f.parameters.weight_phases = 0.5; // NINT(5.5)=6; 4/6 truncated then clamped to 1.
    auto rounded = score_classic(f.input(), f.context);
    ASSERT_TRUE(rounded);
    EXPECT_EQ(rounded.value().expected_peaks_per_phase, 1);
}

TEST(ClassicScore, EmptyCardOutOfRangeAndNoAssociationAreInitialized) {
    ClassicFixture f;
    for (double position : {40.0, 100.0}) {
        f.set_card({{position, 1}});
        auto result = score_classic(f.input(), f.context);
        ASSERT_TRUE(result);
        EXPECT_EQ(result.value().score.total, 0);
        EXPECT_EQ(result.value().score.position, 0);
        EXPECT_EQ(result.value().score.intensity, 0);
        EXPECT_EQ(result.value().score.scale, 1);
    }
    f.set_card({});
    auto result = score_classic(f.input(), f.context);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().disposition, ScoreDisposition::no_reflections_in_range);
}

TEST(ClassicScore, UnmatchedObservedTieSelectsHigherSampleAndPhaseHalfRoundsAway) {
    ClassicFixture f;
    f.set_card({{10, 10}, {20, 20}, {30, 10}, {45, 10}});
    f.pattern.observed[4] = 0;
    f.pattern.observed[5] = 101;
    auto result = score_classic(f.input(), f.context);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().unassociated_below_observed, 1);
    f.set_peaks({{1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1}, {8, 1}, {9, 1}, {10, 1}});
    f.set_card({{1, 1}});
    f.parameters.weight_phases = 0.5;
    result = score_classic(f.input(), f.context);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().expected_peaks_per_phase, 1); // 10/NINT(5.5)=10/6
    f.parameters.weight_phases = 0.51;
    result = score_classic(f.input(), f.context);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().expected_peaks_per_phase, 2); // 10/NINT(5.41)=10/5
}

TEST(ClassicScore, UndefinedLegacyDenominatorsAreErrorsNotSyntheticScores) {
    ClassicFixture f;
    f.set_card({{10, 0}, {20, 0}, {30, 0}});
    auto zero_card = score_classic(f.input(), f.context);
    ASSERT_FALSE(zero_card);
    EXPECT_EQ(zero_card.error().code, ErrorCode::numerical_failure);
    EXPECT_EQ(zero_card.error().reason, "zero_total_intensity");
    f.set_card({{10, 10}});
    f.set_peaks({{10, 0}});
    EXPECT_FALSE(score_classic(f.input(), f.context));
    f.set_peaks({{10, 100}});
    f.parameters.weight_position = f.parameters.weight_phases = 0;
    auto zero_weight = score_classic(f.input(), f.context);
    ASSERT_FALSE(zero_weight); // Single match ignores intensity weight.
    EXPECT_EQ(zero_weight.error().reason, "zero_effective_weight");
    f.parameters.weight_position = 1;
    f.set_peaks({{10, 0}, {20, 100}});
    f.set_card({{10, 0}, {20, 10}});
    auto zero_term = score_classic(f.input(), f.context);
    ASSERT_FALSE(zero_term); // Associated 0/0 intensity term.
    EXPECT_EQ(zero_term.error().reason, "zero_intensity_denominator");
}

TEST(ClassicScore, InvalidInputsAndUnsupportedPolicyAreRejected) {
    ClassicFixture f;
    f.context.numerical_policy = NumericalPolicy::explicit_float64;
    EXPECT_FALSE(score_classic(f.input(), f.context));
    f.context.numerical_policy = NumericalPolicy::legacy_mixed_precision;
    f.parameters.delta_two_theta.value = 0;
    EXPECT_FALSE(score_classic(f.input(), f.context));
    f.parameters.delta_two_theta.value = 0.25;
    f.peaks.peaks[0].intensity = -1;
    EXPECT_FALSE(score_classic(f.input(), f.context));
    f.peaks.peaks[0].intensity = std::numeric_limits<double>::infinity();
    EXPECT_FALSE(score_classic(f.input(), f.context));
    f.peaks.peaks[0].intensity = 100;
    f.card.reflections[0].intensity = 1e30;
    EXPECT_FALSE(score_classic(f.input(), f.context));
    f.card.reflections[0].intensity = 10;
    f.pattern.calculation_range.reset();
    EXPECT_FALSE(score_classic(f.input(), f.context));
}

TEST(ClassicScore, RepeatedIndependentCallsDoNotMutateOrLeakWorkspace) {
    ClassicFixture a, b;
    b.set_card({{40, 1}});
    auto first = score_classic(a.input(), a.context);
    ASSERT_TRUE(first);
    ASSERT_TRUE(score_classic(b.input(), b.context));
    auto second = score_classic(a.input(), a.context);
    ASSERT_TRUE(second);
    EXPECT_EQ(first.value().score.total, second.value().score.total);
    EXPECT_EQ(a.peaks.peaks[0].intensity, 100);
    EXPECT_EQ(a.card.reflections[0].intensity, 10);
    std::stop_source stop;
    stop.request_stop();
    a.context.cancellation = stop.get_token();
    auto cancelled = score_classic(a.input(), a.context);
    ASSERT_FALSE(cancelled);
    EXPECT_EQ(cancelled.error().code, ErrorCode::cancelled);
}
