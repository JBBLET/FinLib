// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

#include "TestMockTimeSeries.hpp"
#include "finlib/core/TimeSeries.hpp"

using ts::InterpolationStrategy;

class TimeSeriesResamplingTest : public TimeSeriesMocks {};

TEST_F(TimeSeriesResamplingTest, ResamplingLinearDeterminism) {
    // simpleSeries: {1,2,3,4,5} at t=1000..5000 — midpoints at 1500 and 2500 interpolate to 1.5 and 2.5
    std::vector<int64_t> target = {1500, 2500};
    auto resampled = simpleSeries->resampling(target, InterpolationStrategy::Linear);
    EXPECT_NEAR(resampled.getValues()[0], 1.5, 1e-9);
    EXPECT_NEAR(resampled.getValues()[1], 2.5, 1e-9);
}

TEST_F(TimeSeriesResamplingTest, StochasticSeedIsDeterministic) {
    std::vector<int64_t> target = {1500};
    auto res1 = simpleSeries->resampling(target, InterpolationStrategy::Stochastic, 42);
    auto res2 = simpleSeries->resampling(target, InterpolationStrategy::Stochastic, 42);
    EXPECT_DOUBLE_EQ(res1.getValues()[0], res2.getValues()[0]);
}

TEST_F(TimeSeriesResamplingTest, ResamplingNearestNeighbour) {
    // simpleSeries: {1,2,3,4,5} at t=1000..5000
    // t=1300 → nearest is t=1000 (val 1.0); t=1700 → nearest is t=2000 (val 2.0)
    std::vector<int64_t> target = {1300, 1700};
    auto resampled = simpleSeries->resampling(target, InterpolationStrategy::Nearest);
    EXPECT_DOUBLE_EQ(resampled.getValues()[0], 1.0);
    EXPECT_DOUBLE_EQ(resampled.getValues()[1], 2.0);
}

TEST_F(TimeSeriesResamplingTest, ResamplingStochasticBounds) {
    // Linear midpoint between {1,2} at {1000,2000} is 1.5 — stochastic should differ
    std::vector<int64_t> target = {1500};
    auto res1 = simpleSeries->resampling(target, InterpolationStrategy::Stochastic);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    auto res2 = simpleSeries->resampling(target, InterpolationStrategy::Stochastic);
    EXPECT_NE(res1.getValues()[0], 1.5);
    EXPECT_NE(res1.getValues()[0], res2.getValues()[0]);
}

TEST_F(TimeSeriesResamplingTest, ParallelBoundaryContinuity) {
    const size_t N = 100000;
    std::vector<int64_t> ts(N);
    std::vector<double> vals(N);
    for (size_t i = 0; i < N; ++i) {
        ts[i] = i * 100;
        vals[i] = static_cast<double>(i);
    }
    TimeSeries large_ts("ContinuityTestTS", std::move(ts), std::move(vals));

    std::vector<int64_t> target_ts(N - 1);
    for (size_t i = 0; i < N - 1; ++i) target_ts[i] = (i * 100) + 50;

    auto result = large_ts.resampling(target_ts, InterpolationStrategy::Linear);
    ASSERT_EQ(result.size(), target_ts.size());

    for (size_t i = 0; i < result.size(); ++i) {
        double expected = static_cast<double>(i) + 0.5;
        ASSERT_NEAR(result.getValues()[i], expected, 1e-9)
            << "Mismatch at index " << i << " across parallel chunk boundary!";
    }
}

TEST_F(TimeSeriesResamplingTest, ParallelSpeedupBenchmark) {
    const size_t N = 1000000;
    std::vector<int64_t> ts(N);
    std::vector<double> vals(N);
    for (size_t i = 0; i < N; ++i) {
        ts[i] = i;
        vals[i] = i;
    }
    TimeSeries large_ts("ParallelTestTS", ts, vals);
    std::vector<int64_t> target = ts;

    auto start = std::chrono::high_resolution_clock::now();
    auto result = large_ts.resampling(target, InterpolationStrategy::Linear);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "[          ] Parallel execution time for 1M points: " << duration << " ms" << std::endl;

    EXPECT_GT(result.size(), 0);
}
