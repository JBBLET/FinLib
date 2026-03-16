// Copyright 2026 JBBLET
#pragma once

#include <Eigen/Dense>
#include <cstddef>
#include <optional>
#include <vector>

#include "Eigen/Core"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/core/stats.hpp"

namespace analysis {

class TimeSeriesAnalysis {
 public:
    explicit TimeSeriesAnalysis(const TimeSeriesView& view) : view_(view) {}

    double mean() const;
    double variance(stats::VarianceType type = stats::VarianceType::Sample) const;
    double std_dev() const;
    double skewness() const;
    double kurtosis() const;

    double autocorrelation(size_t lag) const;
    const std::vector<double>& acf(size_t max_lag) const;
    const std::vector<double>& autocovariances(size_t max_lag) const;

    Eigen::MatrixXd toeplitz(size_t max_lag);
    double z_score(double value) const;
    bool is_outlier(double value, double threshold = 3.0) const;

    void invalidate_cache();

 private:
    TimeSeriesView view_;

    mutable std::optional<double> cached_mean_;
    mutable std::optional<double> cached_var_sample_;
    mutable std::optional<double> cached_var_population_;
    mutable std::optional<double> cached_skew_;
    mutable std::optional<double> cached_kurt_;
    mutable std::optional<std::vector<double>> cached_acf_;
    mutable std::optional<std::vector<double>> cached_autocovariances_;
    mutable std::optional<Eigen::MatrixXd> cached_toeplitz_;
};

}  // namespace analysis
