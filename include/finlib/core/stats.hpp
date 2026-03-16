// Copyright 2026 JBBLET
#pragma once

#include <Eigen/Dense>
#include <vector>
class TimeSeriesView;

namespace analysis::stats {

// Standard
enum class VarianceType { Population, Sample };
double mean(const TimeSeriesView& view);
double variance_fast(const TimeSeriesView& view, VarianceType type = VarianceType::Sample);
double variance_slow(const TimeSeriesView& view, VarianceType type = VarianceType::Sample);
double std_deviation(const TimeSeriesView& view, VarianceType type = VarianceType::Sample);

double skewness(const TimeSeriesView& view);
double kurtosis(const TimeSeriesView& view);
double excess_kurtosis(const TimeSeriesView&);
double autocorrelation_at(const TimeSeriesView& view, size_t lag);
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

HypothesisTestResult jarque_bera(const TimeSeriesView&);
HypothesisTestResult adf(const TimeSeriesView&);
HypothesisTestResult breusch_pagan(const TimeSeriesView&);
HypothesisTestResult breusch_godfrey(const TimeSeriesView&);
}  // namespace analysis::hypothesisTesting

namespace analysis::finance {
double volatility(const TimeSeriesView& view, double frequency = 252.0);
}  //  namespace analysis::finance
