// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include <gtest/gtest.h>

#include <memory>

#include "TestMockTimeSeries.hpp"
#include "finlib/core/StatsCore.hpp"
#include "finlib/core/TimeSeries.hpp"
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
    auto view = series->view();
    EXPECT_EQ(analysis::stats::mean(view), 30.00);
    EXPECT_EQ(analysis::stats::varianceSlow(view, analysis::stats::VarianceType::Population), 200.00);
    EXPECT_EQ(analysis::stats::varianceFast(view, analysis::stats::VarianceType::Population), 200.00);
}
