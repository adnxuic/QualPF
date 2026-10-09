#include "classic_fixture.h"
#include "qualpf/application/classic_search.h"
#include "snapshot_codec.h"

#include <gtest/gtest.h>
#include <limits>
#include <stop_token>

using namespace qualpf;
using namespace qualpf::application;
using namespace qualpf::search;
using namespace qualpf::testing;

namespace {
void load(Session &session, const ClassicFixture &f) {
    auto edit = session.begin_edit();
    ASSERT_TRUE(edit.replace_pattern(f.pattern));
    ASSERT_TRUE(edit.replace_peaks(f.peaks));
    ASSERT_TRUE(edit.set_search_parameters(f.parameters));
    ASSERT_TRUE(session.commit(edit));
}
std::vector<CandidateInput> candidates(const ClassicFixture &f) { return {{f.card, std::nullopt, 10.0}}; }
} // namespace

TEST(ClassicWorkflow, PreparedInputsAndOutputsSurviveCallerMutation) {
    ClassicFixture f;
    Session session;
    load(session, f);
    auto source = session.snapshot();
    auto cards = candidates(f);
    auto search = prepare_classic_search(source, cards, SearchPass::initial, f.context);
    ASSERT_TRUE(search);
    EXPECT_FALSE(session.results());
    cards[0].card.reflections.clear();
    f.pattern.observed.clear();
    f.peaks.peaks.clear();
    ASSERT_TRUE(search.value().publish(session, f.context));
    EXPECT_EQ(session.snapshot(), source);
    ASSERT_EQ(session.results()->candidates.size(), 1);
    EXPECT_DOUBLE_EQ(session.results()->candidates[0].outcome.value().scale, 10);
    EXPECT_EQ(source->inputs.datasets[0].peaks->peaks.size(), 3);
}

TEST(ClassicWorkflow, TransportCopyFeedsSearchWithoutBorrowingLegacyBuffers) {
    ClassicFixture f;
    float x[] = {0, 10, 20, 30, 40, 50, 60};
    float y[] = {0, 100, 200, 100, 0, 0, 0};
    legacy::PatternExportView view;
    view.two_theta = x;
    view.observed = y;
    view.reported_range = f.pattern.calculation_range;
    auto copied = legacy::copy_pattern_export(view, f.pattern.id, "controlled ABI buffer");
    ASSERT_TRUE(copied);
    f.pattern = copied.value().pattern;
    Session session;
    load(session, f);
    y[1] = 999;
    x[1] = 11;
    auto cards = candidates(f);
    auto result = prepare_classic_search(session.snapshot(), cards, SearchPass::initial, f.context);
    ASSERT_TRUE(result);
    ASSERT_TRUE(result.value().publish(session, f.context));
    EXPECT_DOUBLE_EQ(session.results()->candidates[0].outcome.value().total, 1.0);
    EXPECT_DOUBLE_EQ(session.snapshot()->inputs.datasets[0].pattern.observed[1], 100);
    EXPECT_FALSE(session.snapshot()->inputs.datasets[0].pattern.original_observed);
}

TEST(ClassicWorkflow, TwoResidualPhasesPublishAtomicallyAndKeepWorkingSpectrum) {
    ClassicFixture f;
    Session session;
    load(session, f);
    auto source = session.snapshot();
    auto cards = candidates(f);
    auto first = prepare_classic_residual(source, f.card, 5.0, cards, f.context);
    ASSERT_TRUE(first);
    EXPECT_EQ(session.snapshot(), source);
    EXPECT_FALSE(session.results());
    ASSERT_TRUE(first.value().residual_peaks());
    EXPECT_DOUBLE_EQ(first.value().residual_peaks()->peaks[0].intensity, 50);
    EXPECT_DOUBLE_EQ(first.value().evaluation().evaluated[0].outcome.value().scale, 5);
    EXPECT_DOUBLE_EQ(first.value().evaluation().results.candidates[0].outcome.value().scale, 10);
    ASSERT_TRUE(first.value().publish(session, f.context));
    auto residual = session.snapshot();
    EXPECT_EQ(residual->revision, source->revision + 1);
    EXPECT_DOUBLE_EQ(source->inputs.datasets[0].peaks->peaks[0].intensity, 100);
    EXPECT_DOUBLE_EQ(residual->inputs.datasets[0].peaks->peaks[0].original_intensity.value(), 100);
    EXPECT_EQ(residual->inputs.datasets[0].pattern.observed, f.pattern.observed);
    auto second = prepare_classic_residual(residual, f.card, 5.0, cards, f.context);
    ASSERT_TRUE(second);
    ASSERT_TRUE(second.value().publish(session, f.context));
    EXPECT_TRUE(session.snapshot()->inputs.datasets[0].peaks->peaks.empty());
    EXPECT_TRUE(session.results()->candidates.empty());
    EXPECT_TRUE(session.results()->complete);
    EXPECT_FALSE(first.value().publish(session, f.context));
}

TEST(ClassicWorkflow, FailureCancellationStaleAndCrossSessionNeverPublish) {
    ClassicFixture f;
    Session session, other;
    load(session, f);
    load(other, f);
    auto source = session.snapshot();
    auto cards = candidates(f);
    cards[0].previous_scale.reset();
    EXPECT_FALSE(prepare_classic_residual(source, f.card, 5, cards, f.context));
    EXPECT_EQ(session.snapshot(), source);
    EXPECT_FALSE(session.results());
    cards = candidates(f);
    auto prepared = prepare_classic_residual(source, f.card, 5, cards, f.context);
    ASSERT_TRUE(prepared);
    EXPECT_FALSE(prepared.value().publish(other, f.context));
    std::stop_source stop;
    auto cancelled = f.context;
    cancelled.cancellation = stop.get_token();
    stop.request_stop();
    EXPECT_FALSE(prepare_classic_search(source, cards, SearchPass::initial, cancelled));
    EXPECT_FALSE(prepared.value().publish(session, cancelled));
    EXPECT_EQ(session.snapshot(), source);
    auto edit = session.begin_edit();
    ASSERT_TRUE(edit.set_search_parameters(f.parameters));
    ASSERT_TRUE(session.commit(edit));
    EXPECT_FALSE(prepared.value().publish(session, f.context));
    EXPECT_FALSE(session.results());
}

TEST(ClassicWorkflow, InvalidTransitionPreservesBothExistingOwners) {
    ClassicFixture f;
    Session session;
    load(session, f);
    auto cards = candidates(f);
    auto initial = prepare_classic_search(session.snapshot(), cards, SearchPass::initial, f.context);
    ASSERT_TRUE(initial);
    ASSERT_TRUE(initial.value().publish(session, f.context));
    auto source = session.snapshot();
    auto existing = session.results();
    auto invalid = *existing;
    invalid.candidates[0].outcome =
        Result<ScoreValues>{ScoreValues{std::numeric_limits<double>::quiet_NaN(), 0, 0, 1}};
    EXPECT_FALSE(session.publish_search_transition(source, f.peaks, invalid));
    EXPECT_EQ(session.snapshot(), source);
    EXPECT_EQ(session.results(), existing);
    invalid = *existing;
    invalid.complete = false;
    EXPECT_FALSE(session.publish_search_transition(source, f.peaks, invalid));
    EXPECT_EQ(session.results(), existing);
}

TEST(ClassicWorkflow, UndefinedCandidateRejectsWholeSearchAndResidualTransaction) {
    ClassicFixture f;
    Session session;
    load(session, f);
    auto cards = candidates(f);
    auto initial = prepare_classic_search(session.snapshot(), cards, SearchPass::initial, f.context);
    ASSERT_TRUE(initial);
    ASSERT_TRUE(initial.value().publish(session, f.context));
    const auto source = session.snapshot();
    const auto existing = session.results();
    auto bad = cards.front();
    bad.card.card_id = "undefined-zero-intensity";
    for (auto &reflection : bad.card.reflections)
        reflection.intensity = 0;
    cards.push_back(bad); // A finite candidate precedes the invalid one.
    auto search = prepare_classic_search(source, cards, SearchPass::initial, f.context);
    ASSERT_FALSE(search);
    EXPECT_EQ(search.error().code, ErrorCode::numerical_failure);
    EXPECT_EQ(search.error().reason, "zero_total_intensity");
    EXPECT_EQ(session.snapshot(), source);
    EXPECT_EQ(session.results(), existing);
    auto residual = prepare_classic_residual(source, f.card, 5, cards, f.context);
    ASSERT_FALSE(residual);
    EXPECT_EQ(residual.error().reason, "zero_total_intensity");
    EXPECT_EQ(session.snapshot(), source);
    EXPECT_EQ(session.results(), existing);
}

TEST(ClassicWorkflow, ReimportedValuesContinueSearchWithoutSharingSessionIdentity) {
    // In-memory value round-trip, explicitly NOT .qxp serialization coverage.
    ClassicFixture f;
    Session original;
    load(original, f);
    auto cards = candidates(f);
    auto residual = prepare_classic_residual(original.snapshot(), f.card, 5, cards, f.context);
    ASSERT_TRUE(residual);
    ASSERT_TRUE(residual.value().publish(original, f.context));
    auto saved = original.snapshot()->inputs;
    Session restored;
    auto edit = restored.begin_edit();
    for (const auto &d : saved.datasets) {
        ASSERT_TRUE(edit.replace_pattern(d.pattern));
        ASSERT_TRUE(edit.replace_peaks(*d.peaks));
    }
    ASSERT_TRUE(edit.set_search_parameters(*saved.search_parameters));
    ASSERT_TRUE(restored.commit(edit));
    auto a = prepare_classic_search(original.snapshot(), cards, SearchPass::rescore, f.context);
    auto b = prepare_classic_search(restored.snapshot(), cards, SearchPass::rescore, f.context);
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    EXPECT_DOUBLE_EQ(a.value().evaluation().results.candidates[0].outcome.value().total,
                     b.value().evaluation().results.candidates[0].outcome.value().total);
    EXPECT_FALSE(a.value().publish(restored, f.context));
    EXPECT_TRUE(b.value().publish(restored, f.context));
}
