#include "classic_fixture.h"
#include "classic_observation.h"
#include <gtest/gtest.h>

using namespace qualpf;
using namespace qualpf::search;
using namespace qualpf::testing;

TEST(ClassicRegression, MissingReferenceNeverPassesAsZeroOrEmpty) {
    ClassicReference reference;
    auto compared = compare_classic_reference(reference, {});
    ASSERT_FALSE(compared);
    EXPECT_EQ(compared.error().code, ErrorCode::missing_data);
}

TEST(ClassicRegression, ComparatorContractChecksDiscreteAndNumericFieldsIndependently) {
    // Comparator plumbing ONLY. These labels deliberately say synthetic, not
    // Fortran capture.
    ClassicReference reference;
    reference.numerical.evidence = EvidenceState::reviewed;
    reference.numerical.environment = {
        "synthetic",
        "synthetic",
        "none",
        "test",
        "binary32",
        "serial",
        {{"synthetic", std::string(64, '0'), "hand-authored comparator contract"}}};
    NumericalObservation expected;
    expected.fields["total"] = {0.5};
    reference.numerical.expected = expected;
    reference.numerical.tolerances.emplace("total",
                                           FieldTolerance{1e-7, 0, "dimensionless", "synthetic test"});
    reference.discrete["order"] = {"A", "B"};
    ClassicObservation actual{expected, reference.discrete};
    EXPECT_TRUE(compare_classic_reference(reference, actual));
    actual.discrete["order"] = {"B", "A"};
    EXPECT_FALSE(compare_classic_reference(reference, actual));
    actual.discrete = reference.discrete;
    actual.numerical.fields["total"][0] = 0.5001;
    EXPECT_FALSE(compare_classic_reference(reference, actual));
    actual.numerical = expected;
    reference.numerical.tolerances.clear();
    EXPECT_FALSE(compare_classic_reference(reference, actual));
}

TEST(ClassicRegression, HandDerivedTwoPhaseResidualSequence) {
    ClassicFixture f;
    f.set_peaks({{10, 100}, {20, 100}, {30, 100}});
    f.set_card({{10, 10}});
    const auto phase_a = f.card;
    auto residual = subtract_phase(f.peaks, phase_a, 10, f.parameters);
    ASSERT_TRUE(residual);
    ASSERT_EQ(residual.value().peaks.peaks.size(), 2);
    EXPECT_EQ(residual.value().peaks.peaks[0].id, PeakId{2});
    f.set_card({{20, 10}, {30, 10}});
    f.card.card_id = "B";
    std::vector<CandidateInput> candidates{{phase_a, {}, 10}, {f.card, {}, 10}};
    auto batch = evaluate_candidates(f.pattern, residual.value().peaks, candidates, f.parameters,
                                     SearchPass::rescore, f.context);
    ASSERT_TRUE(batch);
    ASSERT_EQ(batch.value().results.candidates.size(), 1);
    EXPECT_EQ(batch.value().results.candidates[0].card_id, "B");
    EXPECT_EQ(batch.value().results.candidates[0].outcome.value().total, 1);
    auto exhausted = subtract_phase(residual.value().peaks, f.card, 10, f.parameters);
    ASSERT_TRUE(exhausted);
    EXPECT_TRUE(exhausted.value().peaks.peaks.empty());
    batch = evaluate_candidates(f.pattern, exhausted.value().peaks, candidates, f.parameters,
                                SearchPass::rescore, f.context);
    ASSERT_TRUE(batch);
    EXPECT_TRUE(batch.value().results.candidates.empty());
}
