#include "classic_fixture.h"
#include <gtest/gtest.h>

using namespace qualpf;
using namespace qualpf::search;
using qualpf::testing::ClassicFixture;

TEST(ClassicResidual, InclusiveAssociationTiesSelectFirstAndQualityCanBeNegative) {
    ClassicFixture f;
    f.set_peaks({{10, 2000}});
    f.set_card({{9.75, 1}, {10.25, 1}});
    auto result = associate_residual(f.peaks, f.card, 1, AngularWidthDegrees{0.25});
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value()[0].reflection_index, 0);
    EXPECT_EQ(result.value()[0].quality, 0);
    f.set_card({{10, 1}});
    result = associate_residual(f.peaks, f.card, 1, AngularWidthDegrees{0.25});
    ASSERT_TRUE(result);
    EXPECT_LT(result.value()[0].quality, 0);
}

TEST(ClassicResidual, ConflictRuleIsNotFomManyToOneAssociation) {
    ClassicFixture f;
    f.set_peaks({{10, 100}, {10.1, 100}});
    f.set_card({{10, 1}});
    auto result = associate_residual(f.peaks, f.card, 1, AngularWidthDegrees{0.25});
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value()[0].reflection_index, 0);
    EXPECT_FALSE(result.value()[1].reflection_index);
    // Peak 1's nearest card is 10.12; its nearest sample is peak 0,
    // which has a better card at 10.05, so peak 1 can claim 10.12.
    f.set_peaks({{10, 100}, {10.3, 100}});
    f.set_card({{10.05, 1}, {10.12, 1}});
    result = associate_residual(f.peaks, f.card, 1, AngularWidthDegrees{0.4});
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value()[1].reflection_index, 1);
}

TEST(ClassicResidual, SubtractionPreservesAllFieldsAndUsesOriginalAcrossPasses) {
    ClassicFixture f;
    f.parameters.weight_intensity = 0; // noise=0.2
    f.set_peaks({{10, 100}, {20, 200}, {30, 100}});
    f.set_card({{10, 60}});
    const auto original = f.peaks;
    auto first = subtract_phase(f.peaks, f.card, 1, f.parameters);
    ASSERT_TRUE(first);
    ASSERT_EQ(first.value().peaks.peaks.size(), 3);
    EXPECT_EQ(first.value().peaks.peaks[0].intensity, 40);
    EXPECT_EQ(first.value().peaks.peaks[0].original_intensity, 100);
    f.set_card({{10, 20}});
    auto second = subtract_phase(first.value().peaks, f.card, 1, f.parameters);
    ASSERT_TRUE(second);
    EXPECT_EQ(second.value().removed, (std::vector<PeakId>{PeakId{1}}));
    ASSERT_EQ(second.value().peaks.peaks.size(), 2);
    const auto &kept = second.value().peaks.peaks[0];
    EXPECT_EQ(kept.id, original.peaks[1].id);
    EXPECT_EQ(kept.d_spacing, original.peaks[1].d_spacing);
    EXPECT_EQ(kept.delta_d, original.peaks[1].delta_d);
    EXPECT_EQ(kept.fwhm, original.peaks[1].fwhm);
    EXPECT_EQ(kept.integrated_intensity, original.peaks[1].integrated_intensity);
    EXPECT_EQ(f.peaks.peaks[0].intensity, 100);
}

TEST(ClassicResidual, DoesNotMergeOverlapsAndFiltersUnmatchedZeros) {
    ClassicFixture f;
    f.set_peaks({{10, 100}, {20, 0}});
    f.set_card({{10, 10}, {10, 20}});
    auto result = subtract_phase(f.peaks, f.card, 2, f.parameters);
    ASSERT_TRUE(result);
    ASSERT_EQ(result.value().peaks.peaks.size(), 1);
    EXPECT_EQ(result.value().peaks.peaks[0].intensity, 80);
    f.set_card({{10, 1000}});
    result = subtract_phase(f.peaks, f.card, 2, f.parameters);
    ASSERT_TRUE(result);
    EXPECT_TRUE(result.value().peaks.peaks.empty());
}

TEST(ClassicResidual, EmptyInputsAndMissingOriginalAreExplicit) {
    ClassicFixture f;
    f.peaks.peaks[0].original_intensity.reset();
    EXPECT_FALSE(subtract_phase(f.peaks, f.card, 1, f.parameters));
    f.peaks.peaks.clear();
    auto result = subtract_phase(f.peaks, f.card, 1, f.parameters);
    ASSERT_TRUE(result);
    EXPECT_TRUE(result.value().peaks.peaks.empty());
    EXPECT_FALSE(associate_residual(f.peaks, f.card, 1, AngularWidthDegrees{0}));
}
