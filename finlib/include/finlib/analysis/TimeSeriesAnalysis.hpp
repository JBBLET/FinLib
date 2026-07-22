// Copyright 2026 JBBLET
#pragma once

#include <Eigen/Dense>
#include <cstddef>
#include <optional>
#include <vector>

#include "Eigen/Core"
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
