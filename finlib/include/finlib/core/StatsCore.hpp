// Copyright 2026 JBBLET
#pragma once

#include <Eigen/Dense>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "finlib/common/Format.hpp"

namespace ts::analysis::stats {

using Samples = std::span<const double>;

// Standard
enum class VarianceType { Population, Sample };

constexpr std::string_view toString(VarianceType type) {
    switch (type) {
        case VarianceType::Population: return "Population";
        case VarianceType::Sample: return "Sample";
    }
    return "<unknown VarianceType>";
}
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

template <>
struct std::formatter<ts::analysis::stats::VarianceType> : std::formatter<std::string_view> {
    auto format(ts::analysis::stats::VarianceType type, std::format_context& ctx) const
        -> std::format_context::iterator {
        return std::formatter<std::string_view>::format(ts::analysis::stats::toString(type), ctx);
    }
};

// A p-value alone invites the reader to eyeball the threshold, so the conventional marker is
// attached here rather than left to each call site to get right.
template <>
struct std::formatter<ts::analysis::hypothesisTesting::HypothesisTestResult> : std::formatter<std::string_view> {
    auto format(const ts::analysis::hypothesisTesting::HypothesisTestResult& result, std::format_context& ctx) const
        -> std::format_context::iterator {
        std::string_view marker = " (n.s.)";
        if (result.p_value < 0.001) {
            marker = " ***";
        } else if (result.p_value < 0.01) {
            marker = " **";
        } else if (result.p_value < 0.05) {
            marker = " *";
        }
        const std::string rendered = std::format("stat={}, p={}{}",
                                                 ts::fmt::formatDouble(result.statistic, 4),
                                                 ts::fmt::formatDouble(result.p_value, 4),
                                                 marker);
        return std::formatter<std::string_view>::format(rendered, ctx);
    }
};
