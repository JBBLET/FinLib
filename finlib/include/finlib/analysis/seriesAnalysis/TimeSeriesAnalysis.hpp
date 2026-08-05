// Copyright 2026 JBBLET
#pragma once

#include <Eigen/Dense>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <vector>

#include "Eigen/Core"
#include "finlib/common/Format.hpp"
#include "finlib/core/StatsCore.hpp"
#include "finlib/core/TimeSeriesView.hpp"

namespace ts::analysis {
class TimeSeriesAnalysis {
 public:
    explicit TimeSeriesAnalysis(const TimeSeriesView& view) : view_(view) {}

    double mean() const;

    // Descriptive statistics that are undefined on too-small windows return an
    // empty optional instead of throwing: sample variance / standard deviation
    // need at least two observations, kurtosis at least four. A non-finite
    // result (e.g. skewness of a constant series) is also reported as empty.
    // Callers treat the empty case as "not available" (N/A).
    std::optional<double> variance(stats::VarianceType type = stats::VarianceType::Sample) const;
    std::optional<double> standardDeviation() const;
    std::optional<double> skewness() const;
    std::optional<double> kurtosis() const;

    // Number of observations backing this analysis (0 when the view is empty).
    size_t size() const { return view_.size(); }

    double autocorrelation(size_t lag) const;
    const std::vector<double>& acf(size_t max_lag) const;
    const std::vector<double>& autocovariances(size_t max_lag) const;

    Eigen::MatrixXd toeplitz(size_t max_lag);
    double zScore(double value) const;
    bool isOutlier(double value, double threshold = 3.0) const;

    void invalidateCache();

    // Display
    // describe() is the whole point of this class from the console: every statistic it
    // already caches, plus the leading autocorrelations, in one table. Statistics that are
    // undefined on this window are reported as N/A rather than omitted, so the reason a
    // number is missing stays visible.
    //
    // Const, but it warms the caches like every other accessor here.
    std::string toString(const fmt::FormatSpec& spec = {}) const;
    void println(const fmt::FormatSpec& spec = {.mode = fmt::FormatMode::Describe}) const;
    void describe(std::size_t autocorrelationLags = 5) const;

 private:
    TimeSeriesView view_;

    mutable std::optional<double> cachedMean_;
    mutable std::optional<double> cachedVarianceSample_;
    mutable std::optional<double> cachedVariancePopulation_;
    mutable std::optional<double> cachedSkewness_;
    mutable std::optional<double> cachedKurtosis_;
    mutable std::optional<std::vector<double>> cachedACF_;
    mutable std::optional<std::vector<double>> cachedAutocovariances_;
    mutable std::optional<Eigen::MatrixXd> cachedToeplitz_;
};

}  // namespace ts::analysis

// {:d} renders the statistics table; the count selects how many autocorrelation lags it
// carries ({:d10}). {} stays a one-liner for logs.
template <>
struct std::formatter<ts::analysis::TimeSeriesAnalysis> {
    ts::fmt::FormatSpec spec;

    constexpr auto parse(std::format_parse_context& ctx) { return ts::fmt::parseFormatSpec(ctx, spec); }

    auto format(const ts::analysis::TimeSeriesAnalysis& analysis, std::format_context& ctx) const
        -> std::format_context::iterator {
        return std::format_to(ctx.out(), "{}", analysis.toString(spec));
    }
};
