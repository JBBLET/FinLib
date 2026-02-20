#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include "finlib/data/implementation/YFinanceProvider.hpp"

TEST(YFinanceProvider, DownloadAAPLLast10Days) {
    YFinanceProvider provider;

    int64_t start_ts = 1704153600000;  // 2024-01-02 UTC
    int64_t end_ts   = 1705363200000;  // 2024-01-16 UTC

    TimeSeries ts = provider.load("AAPL", start_ts, end_ts);

    ASSERT_GT(ts.size(), 0);
    ASSERT_EQ(ts.get_values().size(), ts.size());

    const auto& values = ts.get_values();

    for (double v : values) {
        EXPECT_TRUE(std::isfinite(v));
    }

    const auto& timestamps = ts.get_timestamps();
    EXPECT_TRUE(std::is_sorted(timestamps.begin(), timestamps.end()));
}
