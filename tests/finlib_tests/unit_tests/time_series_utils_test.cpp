// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/utils/TimeSeriesUtils.hpp"
#include "finlib/core/TimeSeries.hpp"

using ts::TimeSeries;
using ts::Timestamp;
using ts::Timestamps;
using ts::TimestampsPtr;

namespace utils = ts::common::utils::timeSeries;

namespace {

constexpr Timestamp kOpenEnded = std::numeric_limits<Timestamp>::max();

TimestampsPtr gridOf(std::vector<Timestamp> ticks) { return std::make_shared<Timestamps>(std::move(ticks)); }

// Compares a series' values tick-by-tick against what the caller expects to read back.
void expectValues(const TimeSeries& series, const std::vector<double>& expected) {
    ASSERT_EQ(series.getValues().size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_DOUBLE_EQ(series.getValues()[i], expected[i]) << "at tick index " << i;
    }
}

// A grid the mask/step tests share: four ticks, evenly spaced.
TimestampsPtr standardGrid() { return gridOf({100, 150, 200, 250}); }

}  // namespace

// ============================================================
// makeRegularTimestamps
// ============================================================

TEST(MakeRegularTimestampsTest, IncludesEndWhenItFallsOnAFrequencyStep) {
    auto grid = utils::makeRegularTimestamps(0, 10, 5);
    EXPECT_EQ(*grid, (Timestamps{0, 5, 10}));
}

TEST(MakeRegularTimestampsTest, StopsBeforeEndWhenItDoesNotFallOnAStep) {
    auto grid = utils::makeRegularTimestamps(0, 10, 3);
    EXPECT_EQ(*grid, (Timestamps{0, 3, 6, 9}));
}

TEST(MakeRegularTimestampsTest, YieldsASingleTickWhenBeginEqualsEnd) {
    auto grid = utils::makeRegularTimestamps(5, 5, 1);
    EXPECT_EQ(*grid, (Timestamps{5}));
}

TEST(MakeRegularTimestampsTest, YieldsOnlyBeginWhenFrequencyExceedsTheSpan) {
    auto grid = utils::makeRegularTimestamps(0, 10, 100);
    EXPECT_EQ(*grid, (Timestamps{0}));
}

// ============================================================
// generateConstantTimeSeries
// ============================================================

TEST(GenerateConstantTimeSeriesTest, RepeatsTheValueOnEveryTick) {
    auto series = utils::generateConstantTimeSeries("c", standardGrid(), 7.5);
    expectValues(series, {7.5, 7.5, 7.5, 7.5});
}

TEST(GenerateConstantTimeSeriesTest, DefaultsToOne) {
    auto series = utils::generateConstantTimeSeries("c", standardGrid());
    expectValues(series, {1.0, 1.0, 1.0, 1.0});
}

TEST(GenerateConstantTimeSeriesTest, ZeroIsAnOrdinaryValue) {
    auto series = utils::generateConstantTimeSeries("c", standardGrid(), 0.0);
    expectValues(series, {0.0, 0.0, 0.0, 0.0});
}

// Callers rely on this to keep derived series arithmetic-compatible with the grid they passed in.
TEST(GenerateConstantTimeSeriesTest, SharesTheCallersGrid) {
    auto grid = standardGrid();
    auto series = utils::generateConstantTimeSeries("c", grid, 1.0);
    EXPECT_EQ(series.getSharedTimestamps(), grid);
}

TEST(GenerateConstantTimeSeriesTest, RangeOverloadLandsOnTheSameTicksAsARegularGrid) {
    auto series = utils::generateConstantTimeSeries("c", 0, 10, 5, 2.0);
    EXPECT_EQ(series.getTimestamps().size(), 3u);
    expectValues(series, {2.0, 2.0, 2.0});
}

// ============================================================
// generateStepSeries
// ============================================================

TEST(GenerateStepSeriesTest, HoldsABreakpointValueUntilTheNextBreakpoint) {
    auto series = utils::generateStepSeries("s", {{150, 5.0}, {250, 9.0}}, standardGrid(), 0.0);
    // ticks: 100 (before first) | 150, 200 (hold 5) | 250 (steps to 9)
    expectValues(series, {0.0, 5.0, 5.0, 9.0});
}

TEST(GenerateStepSeriesTest, UsesFillBeforeAheadOfTheFirstBreakpoint) {
    auto series = utils::generateStepSeries("s", {{200, 7.0}}, standardGrid(), -1.0);
    expectValues(series, {-1.0, -1.0, 7.0, 7.0});
}

TEST(GenerateStepSeriesTest, OrderOfBreakpointsDoesNotMatter) {
    auto shuffled = utils::generateStepSeries("s", {{250, 9.0}, {150, 5.0}}, standardGrid(), 0.0);
    expectValues(shuffled, {0.0, 5.0, 5.0, 9.0});
}

TEST(GenerateStepSeriesTest, LastBreakpointWinsOnADuplicateTimestamp) {
    auto series = utils::generateStepSeries("s", {{150, 5.0}, {150, 9.0}}, standardGrid(), 0.0);
    expectValues(series, {0.0, 9.0, 9.0, 9.0});
}

TEST(GenerateStepSeriesTest, ABreakpointExactlyAtTheGridStartAppliesFromTheFirstTick) {
    auto series = utils::generateStepSeries("s", {{100, 5.0}}, standardGrid(), 0.0);
    expectValues(series, {5.0, 5.0, 5.0, 5.0});
}

TEST(GenerateStepSeriesTest, ABreakpointBeforeTheGridStartAppliesFromTheFirstTick) {
    auto series = utils::generateStepSeries("s", {{50, 5.0}}, standardGrid(), 0.0);
    expectValues(series, {5.0, 5.0, 5.0, 5.0});
}

TEST(GenerateStepSeriesTest, FillBeforeIsUnusedWhenABreakpointAlreadyCoversTheGridStart) {
    auto series = utils::generateStepSeries("s", {{50, 5.0}, {200, 9.0}}, standardGrid(), -1.0);
    expectValues(series, {5.0, 5.0, 9.0, 9.0});
}

// ============================================================
// makeSegmentMask
// ============================================================

TEST(MakeSegmentMaskTest, IsOneOnTheHalfOpenIntervalAndZeroOutside) {
    auto mask = utils::makeSegmentMask(standardGrid(), 150, 250);
    // [150, 250): 150 and 200 are in, 250 is the exclusive upper bound.
    expectValues(mask, {0.0, 1.0, 1.0, 0.0});
}

TEST(MakeSegmentMaskTest, StaysOnThroughTheGridEndWhenOpenEnded) {
    auto mask = utils::makeSegmentMask(standardGrid(), 150, kOpenEnded);
    expectValues(mask, {0.0, 1.0, 1.0, 1.0});
}

TEST(MakeSegmentMaskTest, IsOnFromTheFirstTickWhenTheSegmentStartsAtTheGridStart) {
    auto mask = utils::makeSegmentMask(standardGrid(), 100, 200);
    expectValues(mask, {1.0, 1.0, 0.0, 0.0});
}

TEST(MakeSegmentMaskTest, IsOnFromTheFirstTickWhenTheSegmentStartsBeforeTheGrid) {
    auto mask = utils::makeSegmentMask(standardGrid(), 50, 200);
    expectValues(mask, {1.0, 1.0, 0.0, 0.0});
}

TEST(MakeSegmentMaskTest, CoversTheWholeGridWhenItStartsEarlyAndIsOpenEnded) {
    auto mask = utils::makeSegmentMask(standardGrid(), 50, kOpenEnded);
    expectValues(mask, {1.0, 1.0, 1.0, 1.0});
}

TEST(MakeSegmentMaskTest, IsOffEverywhereWhenTheSegmentStartsAfterTheGridEnds) {
    auto mask = utils::makeSegmentMask(standardGrid(), 300, kOpenEnded);
    expectValues(mask, {0.0, 0.0, 0.0, 0.0});
}

TEST(MakeSegmentMaskTest, IsOffEverywhereWhenTheSegmentClosesBeforeTheGridStarts) {
    auto mask = utils::makeSegmentMask(standardGrid(), 20, 50);
    expectValues(mask, {0.0, 0.0, 0.0, 0.0});
}

// Adjacent segments must tile the grid exactly: at every tick covered by the chain,
// exactly one mask is on. This is what makes mask-and-sum stitching sound.
TEST(MakeSegmentMaskTest, AdjacentSegmentsTileTheGridWithoutGapsOrOverlap) {
    auto grid = standardGrid();
    auto first = utils::makeSegmentMask(grid, 50, 200);
    auto second = utils::makeSegmentMask(grid, 200, kOpenEnded);

    auto sum = first + second;
    expectValues(sum, {1.0, 1.0, 1.0, 1.0});
}
