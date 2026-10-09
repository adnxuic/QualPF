#include "snapshot_codec.h"

#include <gtest/gtest.h>
#include <limits>

using namespace qualpf;
using namespace qualpf::legacy;

TEST(SnapshotCodec, CopiesBuffersWithoutInventingHistoryOrGridAlignment)
{
    float x[] = {10, 20};
    float y[] = {20, 40};
    float background[] = {2};
    PatternExportView source;
    source.two_theta = x;
    source.observed = y;
    source.background = background;
    source.has_background = true;
    source.radiation_code = 77;
    auto copied = copy_pattern_export(source, DatasetId{1}, "fixture");
    ASSERT_TRUE(copied);
    y[0] = 999;
    EXPECT_EQ(copied.value().pattern.observed[0], 20);
    EXPECT_EQ(copied.value().legacy_radiation_code, 77);
    EXPECT_FALSE(copied.value().pattern.radiation.kind.has_value());
    EXPECT_FALSE(copied.value().pattern.background->window.has_value());
    EXPECT_FALSE(copied.value().pattern.original_observed.has_value());
    EXPECT_FALSE(copied.value().pattern.calculation_range.has_value());
}

TEST(SnapshotCodec, RejectsUnequalColumnsAndAbiCountOverflow)
{
    float x[] = {10, 20};
    float y[] = {30};
    PatternExportView source;
    source.two_theta = x;
    source.observed = y;
    EXPECT_FALSE(copy_pattern_export(source, DatasetId{1}, "fixture"));
    const auto too_many = static_cast<std::size_t>(std::numeric_limits<int>::max()) + 1;
    EXPECT_FALSE(checked_abi_count(too_many));
}
