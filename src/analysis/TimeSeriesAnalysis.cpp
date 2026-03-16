// Copyright 2026 JBBLET

#include "finlib/analysis/TimeSeriesAnalysis.hpp"

#include <cmath>
#include <optional>
#include <stdexcept>
#include <vector>

#include "finlib/core/stats.hpp"

namespace analysis {

double TimeSeriesAnalysis::mean() const {
    if (!cached_mean_) {
        cached_mean_ = analysis::stats::mean(view_);
    }
    return cached_mean_.value();
}

double TimeSeriesAnalysis::variance(stats::VarianceType type) const {
    if (type == stats::VarianceType::Sample) {
        if (!cached_var_sample_) {
            cached_var_sample_ = analysis::stats::variance_fast(view_, type);
        }
        return cached_var_sample_.value();
    } else if (type == stats::VarianceType::Population) {
        if (!cached_var_population_) {
            cached_var_population_ = analysis::stats::variance_fast(view_, type);
        }
        return cached_var_population_.value();
    } else {
        throw std::invalid_argument("Invalid Variance type");
    }
}

double TimeSeriesAnalysis::std_dev() const {
    double standard_dev = std::sqrt(variance(stats::VarianceType::Population));
    return standard_dev;
}

double TimeSeriesAnalysis::skewness() const {
    if (!cached_skew_) {
        cached_skew_ = analysis::stats::skewness(view_);
    }
    return cached_skew_.value();
}

double TimeSeriesAnalysis::kurtosis() const {
    if (!cached_kurt_) {
        cached_kurt_ = analysis::stats::kurtosis(view_);
    }
    return cached_kurt_.value();
}

double TimeSeriesAnalysis::autocorrelation(std::size_t lag) const {
    if (cached_acf_ || cached_acf_.value().size() > lag) {
        return cached_acf_.value()[lag];
    } else {
        acf(lag);
        return cached_acf_.value()[lag];
    }
}

const std::vector<double>& TimeSeriesAnalysis::acf(std::size_t max_lag) const {
    if (!cached_acf_ || cached_acf_.value().empty() || (cached_acf_.value().size() - 1) < max_lag) {
        // TODO(JB) Add Optimization of just adding the new ones rather that recalculating everything
        cached_acf_ = stats::acf(view_, max_lag);
    }
    return cached_acf_.value();
}

const std::vector<double>& TimeSeriesAnalysis::autocovariances(size_t max_lag) const {
    if (!cached_autocovariances_ || cached_autocovariances_.value().empty() ||
        (cached_autocovariances_.value().size() - 1) < max_lag) {
        cached_autocovariances_ = analysis::stats::autocovariances(view_, max_lag);
    }
    return cached_autocovariances_.value();
}

Eigen::MatrixXd TimeSeriesAnalysis::toeplitz(size_t max_lag) {
    if (!cached_toeplitz_ || (cached_toeplitz_.value().rows() - 1) < max_lag) {
        if (!cached_autocovariances_ || cached_autocovariances_.value().empty() ||
            (cached_autocovariances_.value().size() - 1) < max_lag) {
            cached_autocovariances_ = analysis::stats::autocovariances(view_, max_lag);
        }
        cached_toeplitz_ = analysis::stats::toeplitz(cached_autocovariances_.value(), max_lag);
    }
    return cached_toeplitz_.value();
}
double TimeSeriesAnalysis::z_score(double value) const {
    double mu = mean();
    double sigma = std_dev();
    if (sigma < 1e-9) return 0.0;
    return (value - mu) / sigma;
}

bool TimeSeriesAnalysis::is_outlier(double value, double threshold) const {
    return std::abs(z_score(value)) > threshold;
}

void TimeSeriesAnalysis::invalidate_cache() {
    cached_mean_.reset();
    cached_var_sample_.reset();
    cached_var_population_.reset();
    cached_kurt_.reset();
    cached_skew_.reset();
    cached_toeplitz_.reset();
    if (cached_acf_) cached_acf_.value().clear();
    cached_acf_.reset();
    if (cached_autocovariances_) cached_autocovariances_.value().clear();
    cached_autocovariances_.reset();
}

}  // namespace analysis
