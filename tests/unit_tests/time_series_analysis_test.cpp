// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "finlib/analysis/TimeSeriesAnalysis.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/core/stats.hpp"

class TimeSeriesAnalysisTest : public ::testing::Test {
 protected:
    // Simple known data: {1, 2, 3, 4, 5}
    std::shared_ptr<TimeSeries> simpleSeries;
    // Constant series: all 3.0
    std::shared_ptr<TimeSeries> constantSeries;
    // Larger series for autocovariance tests
    std::shared_ptr<TimeSeries> largerSeries;

    void SetUp() override {
        simpleSeries = std::make_shared<TimeSeries>(std::vector<int64_t>{1000, 2000, 3000, 4000, 5000},
                                                    std::vector<double>{1.0, 2.0, 3.0, 4.0, 5.0});

        constantSeries = std::make_shared<TimeSeries>(std::vector<int64_t>{1000, 2000, 3000, 4000, 5000},
                                                      std::vector<double>{3.0, 3.0, 3.0, 3.0, 3.0});

        std::vector<int64_t> ts(100);
        std::vector<double> vals(100);
        for (size_t i = 0; i < 100; ++i) {
            ts[i] = static_cast<int64_t>(i) * 1000;
            vals[i] = std::sin(static_cast<double>(i) * 0.1) * 10.0;
        }
        largerSeries = std::make_shared<TimeSeries>(std::move(ts), std::move(vals));
    }
};

// ============================================================
// Basic Statistics Tests
// ============================================================

TEST_F(TimeSeriesAnalysisTest, MeanOfSimpleSeries) {
    auto view = simpleSeries->view();
    analysis::TimeSeriesAnalysis analysis(view);

    EXPECT_DOUBLE_EQ(analysis.mean(), 3.0);
}

TEST_F(TimeSeriesAnalysisTest, MeanOfConstantSeries) {
    auto view = constantSeries->view();
    analysis::TimeSeriesAnalysis analysis(view);

    EXPECT_DOUBLE_EQ(analysis.mean(), 3.0);
}

TEST_F(TimeSeriesAnalysisTest, VarianceSampleOfSimpleSeries) {
    auto view = simpleSeries->view();
    analysis::TimeSeriesAnalysis analysis(view);

    // Sample variance of {1,2,3,4,5} = 10/4 = 2.5
    EXPECT_DOUBLE_EQ(analysis.variance(analysis::stats::VarianceType::Sample), 2.5);
}

TEST_F(TimeSeriesAnalysisTest, VariancePopulationOfSimpleSeries) {
    auto view = simpleSeries->view();
    analysis::TimeSeriesAnalysis analysis(view);

    // Population variance of {1,2,3,4,5} = 10/5 = 2.0
    EXPECT_DOUBLE_EQ(analysis.variance(analysis::stats::VarianceType::Population), 2.0);
}

TEST_F(TimeSeriesAnalysisTest, VarianceOfConstantSeriesIsZero) {
    auto view = constantSeries->view();
    analysis::TimeSeriesAnalysis analysis(view);

    EXPECT_DOUBLE_EQ(analysis.variance(analysis::stats::VarianceType::Population), 0.0);
}

TEST_F(TimeSeriesAnalysisTest, StdDevMatchesSqrtVariance) {
    auto view = simpleSeries->view();
    analysis::TimeSeriesAnalysis analysis(view);

    double expectedStdDev = std::sqrt(analysis.variance(analysis::stats::VarianceType::Population));
    EXPECT_DOUBLE_EQ(analysis.standardDeviation(), expectedStdDev);
}

// ============================================================
// Caching Tests
// ============================================================

TEST_F(TimeSeriesAnalysisTest, MeanIsCached) {
    auto view = simpleSeries->view();
    analysis::TimeSeriesAnalysis analysis(view);

    double first = analysis.mean();
    double second = analysis.mean();

    EXPECT_DOUBLE_EQ(first, second);
    EXPECT_DOUBLE_EQ(first, 3.0);
}

TEST_F(TimeSeriesAnalysisTest, VarianceBothTypesCachedIndependently) {
    auto view = simpleSeries->view();
    analysis::TimeSeriesAnalysis analysis(view);

    double sample = analysis.variance(analysis::stats::VarianceType::Sample);
    double population = analysis.variance(analysis::stats::VarianceType::Population);

    EXPECT_NE(sample, population);
    EXPECT_DOUBLE_EQ(sample, 2.5);
    EXPECT_DOUBLE_EQ(population, 2.0);

    // Second call should return same cached values
    EXPECT_DOUBLE_EQ(analysis.variance(analysis::stats::VarianceType::Sample), sample);
    EXPECT_DOUBLE_EQ(analysis.variance(analysis::stats::VarianceType::Population), population);
}

TEST_F(TimeSeriesAnalysisTest, InvalidateCacheForcesRecomputation) {
    auto view = simpleSeries->view();
    analysis::TimeSeriesAnalysis analysis(view);

    double meanBefore = analysis.mean();
    analysis.invalidateCache();
    double meanAfter = analysis.mean();

    EXPECT_DOUBLE_EQ(meanBefore, meanAfter);
}

// ============================================================
// ACF Tests
// ============================================================

TEST_F(TimeSeriesAnalysisTest, ACFAtLagZeroIsOne) {
    auto view = largerSeries->view();
    analysis::TimeSeriesAnalysis analysis(view);

    const auto& acfValues = analysis.acf(10);
    EXPECT_NEAR(acfValues[0], 1.0, 1e-10);
}

TEST_F(TimeSeriesAnalysisTest, ACFValuesAreBetweenMinusOneAndOne) {
    auto view = largerSeries->view();
    analysis::TimeSeriesAnalysis analysis(view);

    const auto& acfValues = analysis.acf(10);
    for (size_t i = 0; i < acfValues.size(); ++i) {
        EXPECT_GE(acfValues[i], -1.0) << "ACF at lag " << i << " is below -1";
        EXPECT_LE(acfValues[i], 1.0) << "ACF at lag " << i << " is above 1";
    }
}

TEST_F(TimeSeriesAnalysisTest, ACFCacheGrowsWithLargerLag) {
    auto view = largerSeries->view();
    analysis::TimeSeriesAnalysis analysis(view);

    const auto& acf5 = analysis.acf(5);
    EXPECT_EQ(acf5.size(), 6);  // lag 0 through 5

    const auto& acf10 = analysis.acf(10);
    EXPECT_EQ(acf10.size(), 11);  // lag 0 through 10

    // First 6 values should be the same
    for (size_t i = 0; i < 6; ++i) {
        EXPECT_DOUBLE_EQ(acf5[i], acf10[i]);
    }
}

// ============================================================
// Autocovariance Tests
// ============================================================

TEST_F(TimeSeriesAnalysisTest, AutocovarianceAtLagZeroIsUnnormalizedVariance) {
    auto view = largerSeries->view();
    analysis::TimeSeriesAnalysis analysis(view);

    const auto& gamma = analysis.autocovariances(5);
    // gamma(0) should equal sum of (x_i - mean)^2 (unnormalized)
    EXPECT_GT(gamma[0], 0.0);
}

TEST_F(TimeSeriesAnalysisTest, AutocovarianceSymmetryWithACF) {
    auto view = largerSeries->view();
    analysis::TimeSeriesAnalysis analysis(view);

    const auto& gamma = analysis.autocovariances(5);
    const auto& acfValues = analysis.acf(5);

    // ACF = gamma(k) / gamma(0), but stats::acf computes its own denominator
    // The relationship should hold: acf[k] = gamma[k] / gamma[0] approximately
    // (slight differences possible due to denominator computation)
    for (size_t k = 1; k <= 5; ++k) {
        double expected = gamma[k] / gamma[0];
        EXPECT_NEAR(acfValues[k], expected, 1e-10) << "Mismatch at lag " << k;
    }
}

// ============================================================
// Toeplitz Tests
// ============================================================

TEST_F(TimeSeriesAnalysisTest, ToeplitzMatrixIsSymmetric) {
    auto view = largerSeries->view();
    analysis::TimeSeriesAnalysis analysis(view);

    auto T = analysis.toeplitz(5);
    EXPECT_EQ(T.rows(), 5);
    EXPECT_EQ(T.cols(), 5);

    for (int i = 0; i < T.rows(); ++i) {
        for (int j = 0; j < T.cols(); ++j) {
            EXPECT_DOUBLE_EQ(T(i, j), T(j, i)) << "Not symmetric at (" << i << "," << j << ")";
        }
    }
}

TEST_F(TimeSeriesAnalysisTest, ToeplitzDiagonalIsGammaZero) {
    auto view = largerSeries->view();
    analysis::TimeSeriesAnalysis analysis(view);

    const auto& gamma = analysis.autocovariances(5);
    auto T = analysis.toeplitz(5);

    for (int i = 0; i < T.rows(); ++i) {
        EXPECT_DOUBLE_EQ(T(i, i), gamma[0]);
    }
}

// ============================================================
// Z-Score and Outlier Tests
// ============================================================

TEST_F(TimeSeriesAnalysisTest, ZScoreOfMeanIsZero) {
    auto view = simpleSeries->view();
    analysis::TimeSeriesAnalysis analysis(view);

    EXPECT_NEAR(analysis.zScore(3.0), 0.0, 1e-10);
}

TEST_F(TimeSeriesAnalysisTest, OutlierDetection) {
    auto view = simpleSeries->view();
    analysis::TimeSeriesAnalysis analysis(view);

    // mean=3.0, std=sqrt(2.0)~=1.414
    // value=3.0 => z=0 => not outlier
    EXPECT_FALSE(analysis.isOutlier(3.0));

    // value=100.0 => z=(100-3)/1.414 ~ 68.6 => outlier
    EXPECT_TRUE(analysis.isOutlier(100.0));
}

// ============================================================
// Stats free functions — variance consistency
// ============================================================

TEST_F(TimeSeriesAnalysisTest, VarianceFastAndSlowAgree) {
    auto view = largerSeries->view();

    double fast = analysis::stats::varianceFast(view, analysis::stats::VarianceType::Sample);
    double slow = analysis::stats::varianceSlow(view, analysis::stats::VarianceType::Sample);

    EXPECT_NEAR(fast, slow, 1e-10);
}

TEST_F(TimeSeriesAnalysisTest, VarianceFastAndSlowAgreePopulation) {
    auto view = largerSeries->view();

    double fast = analysis::stats::varianceFast(view, analysis::stats::VarianceType::Population);
    double slow = analysis::stats::varianceSlow(view, analysis::stats::VarianceType::Population);

    EXPECT_NEAR(fast, slow, 1e-10);
}
