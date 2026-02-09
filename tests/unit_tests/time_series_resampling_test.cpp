#include "finlib/core/TimeSeries.hpp"
#include <gtest/gtest.h>
#include <chrono>

TEST(TimeSeriesResamplingTest, ResamplingLinearDeterminism) {
    std::vector<int64_t> ts = {100, 200, 300};
    std::vector<double> vals = {100.0, 200.0, 300.0};
    TimeSeries series(ts, vals);

    std::vector<int64_t> target = {150, 250};
    auto resampled = series.resampling(target, InterpolationStrategy::Linear);

    EXPECT_NEAR(resampled.get_values()[0], 150.0, 1e-9);
    EXPECT_NEAR(resampled.get_values()[1], 250.0, 1e-9);
}

TEST(TimeSeriesResamplingTest, StochasticSeedIsDeterministic) {
    std::vector<int64_t> ts = {1000, 2000};
    std::vector<double> vals = {10.0, 20.0};
    TimeSeries series(ts, vals);

    std::vector<int64_t> target = {1500};
    
    auto res1 = series.resampling(target, InterpolationStrategy::Stochastic, 42);
    auto res2 = series.resampling(target, InterpolationStrategy::Stochastic, 42);

    EXPECT_DOUBLE_EQ(res1.get_values()[0], res2.get_values()[0]);
}

TEST(TimeSeriesResamplingTest, ResamplingStochasticBounds) {
    std::vector<int64_t> ts = {1000, 2000};
    std::vector<double> vals = {10.0, 20.0};
    TimeSeries series(ts, vals);

    std::vector<int64_t> target = {1500};
    auto res1 = series.resampling(target, InterpolationStrategy::Stochastic);
    std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Ensure entropy change if using time
    auto res2 = series.resampling(target, InterpolationStrategy::Stochastic);

    EXPECT_NE(res1.get_values()[0], 15.0); 
    EXPECT_NE(res1.get_values()[0], res2.get_values()[0]);
}

TEST(TimeSeriesResamplingTest, ParallelBoundaryContinuity) {
    const size_t N = 100000; // Well above the 20k threshold
    std::vector<int64_t> ts(N);
    std::vector<double> vals(N);

    // Create a series where Value = Timestamp / 100
    for (size_t i = 0; i < N; ++i) {
        ts[i] = i * 100;
        vals[i] = static_cast<double>(i);
    }

    TimeSeries large_ts(std::move(ts), std::move(vals));

    // Resample to the midpoints (e.g., 50, 150, 250...)
    std::vector<int64_t> target_ts(N - 1);
    for (size_t i = 0; i < N - 1; ++i) {
        target_ts[i] = (i * 100) + 50;
    }

    // This triggers the std::async path
    auto result = large_ts.resampling(target_ts, InterpolationStrategy::Linear);

    ASSERT_EQ(result.size(), target_ts.size());

    // Verify every single point. 
    // If chunking is wrong, the error usually appears around index 20,000, 40,000, etc.
    for (size_t i = 0; i < result.size(); ++i) {
        double expected = static_cast<double>(i) + 0.5;
        ASSERT_NEAR(result.get_values()[i], expected, 1e-9) 
            << "Mismatch at index " << i << " across parallel chunk boundary!";
    }
}

TEST(TimeSeriesResamplingTest, ParallelSpeedupBenchmark) {
    const size_t N = 1000000; // 1 Million points
    std::vector<int64_t> ts(N);
    std::vector<double> vals(N);
    for (size_t i = 0; i < N; ++i) { ts[i] = i; vals[i] = i; }
    
    TimeSeries large_ts(ts, vals);
    std::vector<int64_t> target = ts; // Resample to same timestamps for simplicity

    // Measure time
    auto start = std::chrono::high_resolution_clock::now();
    auto result = large_ts.resampling(target, InterpolationStrategy::Linear);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "[          ] Parallel execution time for 1M points: " << duration << " ms" << std::endl;
    
    EXPECT_GT(result.size(), 0);
}
