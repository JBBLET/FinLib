// Copyright 2026 JBBLET
#pragma once

#include <Eigen/Dense>
#include <cstddef>
#include <span>
#include <vector>

namespace ts::analysis::stats {

using Samples = std::span<const double>;

// Standard
enum class VarianceType { Population, Sample };
double mean(Samples x);
double varianceFast(Samples x, VarianceType type = VarianceType::Sample);
double varianceSlow(Samples x, VarianceType type = VarianceType::Sample);
double standardDeviation(Samples x, VarianceType type = VarianceType::Sample);

double skewness(Samples x);
double kurtosis(Samples x);
double excessKurtosis(Samples x);

double quantileSorted(Samples sortedX, double q);

double autocorrelationAt(Samples x, std::size_t lag);
std::vector<double> acf(Samples x, std::size_t maxLag);
std::vector<double> pacf(Samples x, std::size_t maxLag);
std::vector<double> autocovariances(Samples x, std::size_t maxLag);

Eigen::MatrixXd toeplitzFromSamples(Samples x, std::size_t maxLag);
Eigen::MatrixXd toeplitzFromAutocovariances(Samples gamma, std::size_t maxLag);
}  // namespace ts::analysis::stats

namespace ts::analysis::hypothesisTesting {

struct HypothesisTestResult {
    double statistic;
    double p_value;
};

HypothesisTestResult jarqueBera(stats::Samples x);
HypothesisTestResult adf(stats::Samples x);
HypothesisTestResult breuschPagan(stats::Samples x);
HypothesisTestResult breuschGodfrey(stats::Samples x);

double PvalueFromTStatistic(double tStat);
}  // namespace ts::analysis::hypothesisTesting
