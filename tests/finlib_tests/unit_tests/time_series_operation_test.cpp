// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include <gtest/gtest.h>

#include "TestMockTimeSeries.hpp"
#include "finlib/core/TimeSeries.hpp"

class TimeSeriesOperatorsTest : public TimeSeriesMocks {};

TEST_F(TimeSeriesOperatorsTest, ScalarMultiplication) {
    TimeSeries result = *decadeSeries * 2.0;
    EXPECT_DOUBLE_EQ(result.getValues()[0], 20.0);
    EXPECT_DOUBLE_EQ(result.getValues()[4], 100.0);
    EXPECT_EQ(result.getSharedTimestamps(), decadeSeries->getSharedTimestamps());
}

TEST_F(TimeSeriesOperatorsTest, ScalarAdditionInPlace) {
    TimeSeries series = *decadeSeries;
    series += 5.0;
    EXPECT_DOUBLE_EQ(series.getValues()[0], 15.0);
    EXPECT_DOUBLE_EQ(series.getValues()[4], 55.0);
}

TEST_F(TimeSeriesOperatorsTest, SeriesAddition) {
    // decadeSeries {10,20,30,40,50} + simpleSeries {1,2,3,4,5} = {11,22,33,44,55}
    TimeSeries result = *decadeSeries + *simpleSeries;
    EXPECT_DOUBLE_EQ(result.getValues()[0], 11.0);
    EXPECT_DOUBLE_EQ(result.getValues()[4], 55.0);
    EXPECT_EQ(result.getSharedTimestamps(), decadeSeries->getSharedTimestamps());
}

TEST_F(TimeSeriesOperatorsTest, SeriesMultiplicationInPlace) {
    // decadeSeries {10,20,30,40,50} *= simpleSeries {1,2,3,4,5} = {10,40,90,160,250}
    TimeSeries s1 = *decadeSeries;
    s1 *= *simpleSeries;
    EXPECT_DOUBLE_EQ(s1.getValues()[0], 10.0);
    EXPECT_DOUBLE_EQ(s1.getValues()[4], 250.0);
}

TEST_F(TimeSeriesOperatorsTest, ThrowsOnMismatchedTimestamps) {
    auto s1 = makeSeriesAt("s1", {1000, 2000}, {10.0, 20.0});
    auto s2 = makeSeriesAt("s2", {1000, 2001}, {1.0, 2.0});
    EXPECT_THROW(*s1 + *s2, std::invalid_argument);
    EXPECT_THROW(*s1 *= *s2, std::invalid_argument);
}
