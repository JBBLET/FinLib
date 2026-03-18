// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

#include "finlib/data/implementation/YFinanceProvider.hpp"

TEST(YFinanceProvider, DownloadAAPLLast10Days) {
    YFinanceProvider provider;

    int64_t startTs = 1704153600000;  // 2024-01-02 UTC
    int64_t endTs = 1705363200000;    // 2024-01-16 UTC

    TimeSeries ts = provider.load("AAPL", startTs, endTs);

    ASSERT_GT(ts.size(), 0);
    ASSERT_EQ(ts.getValues().size(), ts.size());

    const auto& values = ts.getValues();

    for (double v : values) {
        EXPECT_TRUE(std::isfinite(v));
    }

    const auto& timestamps = ts.getTimestamps();
    EXPECT_TRUE(std::is_sorted(timestamps.begin(), timestamps.end()));
}
