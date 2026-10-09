#include "classic_fixture.h"
#include <gtest/gtest.h>
#include <limits>

using namespace qualpf;
using namespace qualpf::search;
using qualpf::testing::ClassicFixture;

TEST(ClassicCandidates, StrongestWindowIsInclusiveAndAllowsReuse) {
    ClassicFixture f;
    f.peaks.peaks[0].d_spacing = DSpacingAngstrom{4};
    f.peaks.peaks[1].d_spacing = DSpacingAngstrom{3};
    f.peaks.peaks[2].d_spacing = DSpacingAngstrom{2};
    f.peaks.peaks[0].delta_d = DSpacingAngstrom{0.125};
    const std::vector<DSpacingAngstrom> top{DSpacingAngstrom{4.125}, DSpacingAngstrom{4}, DSpacingAngstrom{4},
                                            DSpacingAngstrom{999}};
    auto r = passes_strongest(f.peaks, top);
    ASSERT_TRUE(r);
    EXPECT_TRUE(r.value()); // First three, not one-to-one matching.
    f.peaks.peaks.resize(2);
    r = passes_strongest(f.peaks, top);
    ASSERT_TRUE(r);
    EXPECT_FALSE(r.value());
    r = passes_strongest(f.peaks, {});
    ASSERT_TRUE(r);
    EXPECT_TRUE(r.value());
}

TEST(ClassicCandidates, NonuniformToleranceRetainsLegacyLimitedProbe) {
    ClassicFixture f;
    f.set_peaks({{1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1}});
    for (std::size_t i = 0; i < 7; ++i) {
        f.peaks.peaks[i].d_spacing = DSpacingAngstrom{10 - double(i)};
        f.peaks.peaks[i].delta_d = DSpacingAngstrom{0};
    }
    f.peaks.peaks[0].delta_d = DSpacingAngstrom{10};
    const std::vector<DSpacingAngstrom> top{DSpacingAngstrom{4.5}};
    auto result = passes_strongest(f.peaks, top);
    ASSERT_TRUE(result);
    EXPECT_FALSE(result.value()); // A full scan would incorrectly pass.
}

TEST(ClassicCandidates, MissingSpacingAndUnsortedSpacingAreDiagnosed) {
    ClassicFixture f;
    f.peaks.peaks[0].delta_d.reset();
    EXPECT_FALSE(passes_strongest(f.peaks, {}));
    f.peaks.peaks[0].delta_d = DSpacingAngstrom{0.1};
    f.peaks.peaks[0].d_spacing = DSpacingAngstrom{0.1};
    EXPECT_FALSE(passes_strongest(f.peaks, {}));
}

TEST(ClassicCandidates, InitialAndRescoreThresholdAndScaleAreDistinct) {
    ClassicFixture f;
    std::vector<CandidateInput> cards{{f.card, std::nullopt, 7.0}};
    f.parameters.minimum_score = 1;
    auto initial =
        evaluate_candidates(f.pattern, f.peaks, cards, f.parameters, SearchPass::initial, f.context);
    ASSERT_TRUE(initial);
    EXPECT_TRUE(initial.value().results.candidates.empty());
    auto rescore =
        evaluate_candidates(f.pattern, f.peaks, cards, f.parameters, SearchPass::rescore, f.context);
    ASSERT_TRUE(rescore);
    ASSERT_EQ(rescore.value().results.candidates.size(), 1);
    EXPECT_EQ(rescore.value().results.candidates[0].outcome.value().scale, 7);
    EXPECT_EQ(rescore.value().evaluated[0].outcome.value().scale, 10);
}

TEST(ClassicCandidates, InitialOnlySortsOnTruncationAndTiesAreStable) {
    ClassicFixture f;
    auto weak = f.card;
    weak.card_id = "weak";
    weak.reflections.pop_back();
    auto perfect = f.card;
    perfect.card_id = "perfect";
    auto tie = perfect;
    tie.card_id = "tie";
    std::vector<CandidateInput> cards{{weak, {}, 1}, {perfect, {}, 1}, {tie, {}, 1}};
    auto run = [&](SearchPass pass) {
        return evaluate_candidates(f.pattern, f.peaks, cards, f.parameters, pass, f.context);
    };
    auto result = run(SearchPass::initial);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().results.candidates[0].card_id, "weak");
    result = run(SearchPass::rescore);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.value().results.candidates[0].card_id, "perfect");
    EXPECT_EQ(result.value().results.candidates[1].card_id, "tie");
    f.parameters.maximum_entries = 1;
    result = run(SearchPass::initial);
    ASSERT_TRUE(result);
    ASSERT_EQ(result.value().results.candidates.size(), 1);
    EXPECT_EQ(result.value().results.candidates[0].card_id, "perfect");
    f.parameters.maximum_entries = 0;
    result = run(SearchPass::initial);
    ASSERT_TRUE(result);
    EXPECT_TRUE(result.value().results.candidates.empty());
}

TEST(ClassicCandidates, PrefilterDisabledDeletedQualityAndRescoreBypass) {
    ClassicFixture f;
    f.card.quality = " D ";
    std::vector<CandidateInput> cards{{f.card, {}, 10}};
    f.parameters.include_deleted_cards = false;
    auto result =
        evaluate_candidates(f.pattern, f.peaks, cards, f.parameters, SearchPass::initial, f.context);
    ASSERT_TRUE(result);
    EXPECT_FALSE(result.value().prefilter_passed[0]);
    f.parameters.filter_strongest = true;
    result = evaluate_candidates(f.pattern, f.peaks, cards, f.parameters, SearchPass::rescore, f.context);
    ASSERT_TRUE(result);
    EXPECT_TRUE(result.value().prefilter_passed[0]);
    cards[0].card.quality = "A";
    EXPECT_FALSE(
        evaluate_candidates(f.pattern, f.peaks, cards, f.parameters, SearchPass::initial, f.context));
    cards[0].stored_top = std::vector<DSpacingAngstrom>{DSpacingAngstrom{999}};
    result = evaluate_candidates(f.pattern, f.peaks, cards, f.parameters, SearchPass::initial, f.context);
    ASSERT_TRUE(result);
    EXPECT_FALSE(result.value().prefilter_passed[0]);
    f.parameters.filter_strongest = false;
    result = evaluate_candidates(f.pattern, f.peaks, cards, f.parameters, SearchPass::initial, f.context);
    ASSERT_TRUE(result);
    EXPECT_TRUE(result.value().prefilter_passed[0]);
}

TEST(ClassicCandidates, ErrorsAreRetainedAndEmptyResidualDropsEveryCard) {
    ClassicFixture f;
    auto zero = f.card;
    for (auto &r : zero.reflections)
        r.intensity = 0;
    std::vector<CandidateInput> cards{{zero, {}, 1}};
    auto result =
        evaluate_candidates(f.pattern, f.peaks, cards, f.parameters, SearchPass::initial, f.context);
    ASSERT_TRUE(result);
    EXPECT_FALSE(result.value().evaluated[0].outcome);
    EXPECT_TRUE(result.value().results.candidates.empty());
    f.peaks.peaks.clear();
    result = evaluate_candidates(f.pattern, f.peaks, cards, f.parameters, SearchPass::rescore, f.context);
    ASSERT_TRUE(result);
    EXPECT_TRUE(result.value().results.candidates.empty());
    EXPECT_TRUE(result.value().results.complete);
    cards.push_back(cards[0]);
    EXPECT_FALSE(
        evaluate_candidates(f.pattern, f.peaks, cards, f.parameters, SearchPass::rescore, f.context));
}
