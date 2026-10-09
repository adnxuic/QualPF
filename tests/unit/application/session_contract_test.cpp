#include "qualpf/application/session.h"

#include <gtest/gtest.h>

using namespace qualpf;
using namespace qualpf::application;

namespace {
Pattern pattern(double intensity = 10.0)
{
    Pattern value;
    value.id = {1};
    value.two_theta = {TwoThetaDegrees{10}, TwoThetaDegrees{20}};
    value.observed = {intensity, 20};
    return value;
}
}

TEST(SessionContract, CancelAndStaleEditsCannotChangeCommittedState)
{
    Session session;
    auto cancelled = session.begin_edit();
    ASSERT_TRUE(cancelled.replace_pattern(pattern()));
    cancelled.cancel();
    EXPECT_FALSE(session.commit(cancelled));
    EXPECT_TRUE(session.snapshot()->inputs.datasets.empty());
    auto first = session.begin_edit();
    auto stale = session.begin_edit();
    ASSERT_TRUE(first.replace_pattern(pattern()));
    ASSERT_TRUE(session.commit(first));
    EXPECT_FALSE(session.commit(first));
    EXPECT_FALSE(session.commit(stale));
    EXPECT_EQ(session.snapshot()->revision, 1u);
}

TEST(SessionContract, ReplacingPatternInvalidatesPeaksAndResultsButKeepsOldSnapshotAlive)
{
    Session session;
    auto edit = session.begin_edit();
    ASSERT_TRUE(edit.replace_pattern(pattern()));
    PeakSet peaks;
    peaks.dataset = {1};
    ASSERT_TRUE(edit.replace_peaks(peaks));
    ASSERT_TRUE(session.commit(edit));
    auto original = session.snapshot();
    SearchResults results;
    results.dataset = {1};
    ASSERT_TRUE(session.publish(original, results));
    auto next = session.begin_edit();
    ASSERT_TRUE(next.replace_pattern(pattern(40)));
    ASSERT_TRUE(session.commit(next));
    EXPECT_EQ(original->inputs.datasets[0].pattern.observed[0], 10);
    EXPECT_EQ(session.snapshot()->inputs.datasets[0].pattern.observed[0], 40);
    EXPECT_FALSE(session.snapshot()->inputs.datasets[0].peaks.has_value());
    EXPECT_FALSE(session.results());
    EXPECT_FALSE(session.publish(original, results));
    EXPECT_FALSE(session.publish_peaks(original, peaks));
    EXPECT_FALSE(session.publish_pattern(original, pattern()));
}

TEST(SessionContract, IdenticalRevisionNumbersDoNotAllowCrossSessionPublication)
{
    Session left, right;
    auto a = left.begin_edit();
    auto b = right.begin_edit();
    ASSERT_TRUE(a.replace_pattern(pattern()));
    ASSERT_TRUE(b.replace_pattern(pattern()));
    ASSERT_TRUE(left.commit(a));
    ASSERT_TRUE(right.commit(b));
    SearchResults result;
    result.dataset = {1};
    EXPECT_FALSE(right.publish(left.snapshot(), result));
    PeakSet peaks;
    peaks.dataset = {1};
    EXPECT_FALSE(right.publish_peaks(left.snapshot(), peaks));
    auto foreign = left.begin_edit();
    EXPECT_FALSE(right.commit(foreign));
}
