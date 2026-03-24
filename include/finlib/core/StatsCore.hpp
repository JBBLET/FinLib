// Copyright 2026 JBBLET
#pragma once

#include <Eigen/Dense>
#include <vector>
class TimeSeriesView;

namespace analysis::stats {

// Standard
enum class VarianceType { Population, Sample };
double mean(const TimeSeriesView& view);
double varianceFast(const TimeSeriesView& view, VarianceType type = VarianceType::Sample);
double varianceSlow(const TimeSeriesView& view, VarianceType type = VarianceType::Sample);
double standardDeviation(const TimeSeriesView& view, VarianceType type = VarianceType::Sample);

double skewness(const TimeSeriesView& view);
double kurtosis(const TimeSeriesView& view);
double excessKurtosis(const TimeSeriesView&);
double autocorrelationAt(const TimeSeriesView& view, size_t lag);
std::vector<double> acf(const TimeSeriesView& view, size_t max_lag);
std::vector<double> pacf(const TimeSeriesView& view, size_t max_lag);
std::vector<double> autocovariances(const TimeSeriesView& view, size_t max_lag);

Eigen::MatrixXd toeplitz(const TimeSeriesView& view, size_t max_lag);
Eigen::MatrixXd toeplitz(const std::vector<double>& gamma, size_t max_lag);
}  // namespace analysis::stats

namespace analysis::hypothesisTesting {
// Test for guaussian distribution, Trend, seasonality and so on
struct HypothesisTestResult {
    double statistic;
    double p_value;
};

HypothesisTestResult jarqueBera(const TimeSeriesView&);
HypothesisTestResult adf(const TimeSeriesView&);
HypothesisTestResult breuschPagan(const TimeSeriesView&);
HypothesisTestResult breuschGodfrey(const TimeSeriesView&);

double PvalueFromTStatistic(double tStat);
}  // namespace analysis::hypothesisTesting

namespace analysis::finance {
double volatility(const TimeSeriesView& view, double frequency = 252.0);
}  //  namespace analysis::finance
