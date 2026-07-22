// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include <gtest/gtest.h>

#include <memory>

#include "TestMockTimeSeries.hpp"
#include "finlib/core/StatsCore.hpp"
#include "finlib/core/TimeSeriesView.hpp"

class TimeSeriesStatsTest : public TimeSeriesMocks {
 protected:
    std::shared_ptr<TimeSeries> series;

    void SetUp() override {
        TimeSeriesMocks::SetUp();
        series = decadeSeries;
    }
};

TEST_F(TimeSeriesStatsTest, TimeSeriesProducesFullView) {
    ts::TimeSeriesView view = series->view();
    EXPECT_EQ(ts::analysis::stats::mean(view), 30.00);
    EXPECT_EQ(ts::analysis::stats::varianceSlow(view, ts::analysis::stats::VarianceType::Population), 200.00);
    EXPECT_EQ(ts::analysis::stats::varianceFast(view, ts::analysis::stats::VarianceType::Population), 200.00);
}
