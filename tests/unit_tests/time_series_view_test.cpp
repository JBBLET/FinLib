#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"

class TimeSeriesViewTest : public ::testing::Test {
 protected:
    std::vector<int64_t> ts = {1000, 2000, 3000, 4000, 5000};
    std::vector<double> vals = {10.0, 20.0, 30.0, 40.0, 50.0};
    std::shared_ptr<TimeSeries> series;

    void SetUp() override {
        series = std::make_shared<TimeSeries>(ts, vals);
    }
};

TEST_F(TimeSeriesViewTest, TimeSeriesProducesFullView) {
    auto view = series->view();
    EXPECT_EQ(view.size(), 5);
    EXPECT_EQ(view[0], 10.0);
    EXPECT_EQ(view.timestamp(4), 5000);
}

TEST_F(TimeSeriesViewTest, TimeSeriesSliceIndexCorrectness) {
    // Slice from index 1 to 3 (inclusive: 2000, 3000, 4000)
    auto view = series->slice_index(1, 3);
    EXPECT_EQ(view.size(), 3);
    EXPECT_EQ(view.timestamp(0), 2000);
    EXPECT_EQ(view.timestamp(2), 4000);
    EXPECT_EQ(view[0], 20.0);
}

TEST_F(TimeSeriesViewTest, ScalarArithmeticMaterialization) {
    auto view = series->slice(1, 3);  // {20, 30, 40}
    TimeSeries result = view + 5.0;   // Should be {25, 35, 45}

    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result.get_values()[0], 25.0);
    EXPECT_EQ(result.get_timestamps()[0], 2000);  // Check that timestamps materialized correctly
}

TEST_F(TimeSeriesViewTest, LagShiftLogic) {
    // Current: {20, 30, 40} at timestamps {2000, 3000, 4000}
    auto v_now = series->slice(1, 3);
    // Lag 1: Should see {10, 20, 30} at the SAME timestamps {2000, 3000, 4000}
    auto v_past = v_now.shift(1);

    EXPECT_EQ(v_past.size(), 3);
    EXPECT_EQ(v_past.timestamp(0), 2000);  // Timestamp doesn't change
    EXPECT_EQ(v_past[0], 10.0);            // Value is "yesterday's"
    EXPECT_EQ(v_past[2], 30.0);
}

TEST_F(TimeSeriesViewTest, ViewSubtractionLogReturnsStyle) {
    auto v_now = series->slice(1, 3);  // indices {1, 2, 3} -> vals {20, 30, 40}
    auto v_past = v_now.shift(1);      // indices {0, 1, 2} -> vals {10, 20, 30}

    // This mimics: Diff = Price_t - Price_t-1
    TimeSeries diff = v_now - v_past;

    EXPECT_EQ(diff.size(), 3);
    EXPECT_EQ(diff.get_values()[0], 10.0);  // 20 - 10
    EXPECT_EQ(diff.get_values()[1], 10.0);  // 30 - 20
    EXPECT_EQ(diff.get_values()[2], 10.0);  // 40 - 30
    EXPECT_EQ(diff.get_timestamps()[0], 2000);
}

TEST_F(TimeSeriesViewTest, OutOfBoundsLagThrows) {
    auto full_view = series->view();
    // Cannot lag the very first element by 1 (nothing exists at index -1)
    EXPECT_THROW(full_view.shift(1), std::out_of_range);
}

TEST_F(TimeSeriesViewTest, AlignmentMismatchThrows) {
    auto view1 = series->slice(0, 3);
    auto view2 = series->slice(1, 3);  // Same length, different timestamps

    // is_aligned_with should return false, operator+ should throw
    EXPECT_THROW(view1 + view2, std::runtime_error);
}
