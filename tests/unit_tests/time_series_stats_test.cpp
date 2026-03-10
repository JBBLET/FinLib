// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/core/stats.hpp"

class TimeSeriesStatsTest : public ::testing::Test {
 protected:
    std::vector<int64_t> ts = {1000, 2000, 3000, 4000, 5000};
    std::vector<double> vals = {10.0, 20.0, 30.0, 40.0, 50.0};
    std::shared_ptr<TimeSeries> series;

    void SetUp() override { series = std::make_shared<TimeSeries>(ts, vals); }
};

TEST_F(TimeSeriesStatsTest, TimeSeriesProducesFullView) {
    auto view = series->view();
    EXPECT_EQ(analysis::stats::mean(view), 30.00);
    EXPECT_EQ(analysis::stats::variance_slow(view, analysis::stats::VarianceType::Population), 200.00);
    EXPECT_EQ(analysis::stats::variance_fast(view, analysis::stats::VarianceType::Population), 200.00);
}
