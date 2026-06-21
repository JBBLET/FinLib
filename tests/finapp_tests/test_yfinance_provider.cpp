// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>

#include "finapp/data/providers/implementations/Yfinance/YFinanceProvider.hpp"

TEST(YFinanceProvider, DownloadAAPLLast10Days) {
    finapp::YFinanceProvider provider("/home/jbblet/user/Documents/Projects/FinLib/.venv/bin/python",
                                      "/home/jbblet/user/Documents/Projects/FinLib/finapp/scripts/YFinanceFetcher.py");

    // Use a rolling window in the last 7 days so 1m data is always available.
    constexpr int64_t kDayMs = 86'400'000LL;
    int64_t endTs =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    int64_t startTs = endTs - 5 * kDayMs;

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
