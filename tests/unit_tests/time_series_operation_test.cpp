#include <gtest/gtest.h>
#include "finlib/core/TimeSeries.hpp"

class TimeSeriesOperatorsTest : public ::testing::Test {
protected:
    std::vector<int64_t> ts1 = {100, 200, 300};
    std::vector<double> v1 = {10.0, 20.0, 30.0};
    std::vector<double> v2 = {1.0, 2.0, 3.0};
};


TEST_F(TimeSeriesOperatorsTest, ScalarMultiplication) {
    TimeSeries series(ts1, v1);
    double scalar = 2.0;

    TimeSeries result = series * scalar;
    EXPECT_DOUBLE_EQ(result.get_values()[0], 20.0);
    EXPECT_DOUBLE_EQ(result.get_values()[2], 60.0);

    EXPECT_EQ(result.get_shared_timestamps(), series.get_shared_timestamps());
}

TEST_F(TimeSeriesOperatorsTest, ScalarAdditionInPlace) {
    TimeSeries series(ts1, v1);
    series += 5.0;
    EXPECT_DOUBLE_EQ(series.get_values()[0], 15.0);
    EXPECT_DOUBLE_EQ(series.get_values()[2], 35.0);
}


TEST_F(TimeSeriesOperatorsTest, SeriesAddition) {
    TimeSeries s1(ts1, v1);
    TimeSeries s2(ts1, v2);
    TimeSeries result = s1 + s2;

    EXPECT_DOUBLE_EQ(result.get_values()[0], 11.0);
    EXPECT_DOUBLE_EQ(result.get_values()[2], 33.0);
    EXPECT_EQ(result.get_shared_timestamps(), s1.get_shared_timestamps());
}

TEST_F(TimeSeriesOperatorsTest, SeriesMultiplicationInPlace) {
    TimeSeries s1(ts1, v1);
    TimeSeries s2(ts1, v2);

    s1 *= s2;

    EXPECT_DOUBLE_EQ(s1.get_values()[0], 10.0);
    EXPECT_DOUBLE_EQ(s1.get_values()[2], 90.0);
}


TEST_F(TimeSeriesOperatorsTest, ThrowsOnMismatchedTimestamps) {
    TimeSeries s1({100, 200}, {10.0, 20.0});
    TimeSeries s2({100, 201}, {1.0, 2.0});

    EXPECT_THROW(s1 + s2, std::invalid_argument);
    EXPECT_THROW(s1 *= s2, std::invalid_argument);
}


TEST_F(TimeSeriesOperatorsTest, ShiftForwardTest) {
    TimeSeries series(ts1, v1); 
    TimeSeries shifted = series.shift(1);

    ASSERT_EQ(shifted.size(), 2);
    EXPECT_EQ(shifted.get_timestamps()[0], 200);
    EXPECT_DOUBLE_EQ(shifted.get_values()[0], 10.0);
    EXPECT_DOUBLE_EQ(shifted.get_values()[1], 20.0);
}
