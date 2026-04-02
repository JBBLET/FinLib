// Copyright 2026 JBBLET

#include "finlib/analysis/TimeSeriesAnalysis.hpp"

#include <cmath>
#include <stdexcept>
#include <vector>

#include "finlib/core/StatsCore.hpp"

namespace analysis {

double TimeSeriesAnalysis::mean() const {
    if (!cachedMean_) {
        cachedMean_ = analysis::stats::mean(view_);
    }
    return cachedMean_.value();
}

double TimeSeriesAnalysis::variance(stats::VarianceType type) const {
    if (type == stats::VarianceType::Sample) {
        if (!cachedVarianceSample_) {
            cachedVarianceSample_ = analysis::stats::varianceFast(view_, type);
        }
        return cachedVarianceSample_.value();
    } else if (type == stats::VarianceType::Population) {
        if (!cachedVariancePopulation_) {
            cachedVariancePopulation_ = analysis::stats::varianceFast(view_, type);
        }
        return cachedVariancePopulation_.value();
    } else {
        throw std::invalid_argument("Invalid Variance type");
    }
}

double TimeSeriesAnalysis::standardDeviation() const {
    double standardDeviation = std::sqrt(variance(stats::VarianceType::Population));
    return standardDeviation;
}

double TimeSeriesAnalysis::skewness() const {
    if (!cachedSkewness_) {
        cachedSkewness_ = analysis::stats::skewness(view_);
    }
    return cachedSkewness_.value();
}

double TimeSeriesAnalysis::kurtosis() const {
    if (!cachedKurtosis_) {
        cachedKurtosis_ = analysis::stats::kurtosis(view_);
    }
    return cachedKurtosis_.value();
}

double TimeSeriesAnalysis::autocorrelation(std::size_t lag) const {
    if (cachedACF_ && cachedACF_.value().size() > lag) {
        return cachedACF_.value()[lag];
    } else {
        acf(lag);
        return cachedACF_.value()[lag];
    }
}

const std::vector<double>& TimeSeriesAnalysis::acf(std::size_t maxLag) const {
    if (!cachedACF_ || cachedACF_.value().empty() || (cachedACF_.value().size() - 1) < maxLag) {
        // TODO(JB) Add Optimization of just adding the new ones rather that recalculating everything
        cachedACF_ = stats::acf(view_, maxLag);
    }
    return cachedACF_.value();
}

const std::vector<double>& TimeSeriesAnalysis::autocovariances(size_t maxLag) const {
    if (!cachedAutocovariances_ || cachedAutocovariances_.value().empty() ||
        (cachedAutocovariances_.value().size() - 1) < maxLag) {
        cachedAutocovariances_ = analysis::stats::autocovariances(view_, maxLag);
    }
    return cachedAutocovariances_.value();
}

Eigen::MatrixXd TimeSeriesAnalysis::toeplitz(size_t maxLag) {
    if (!cachedToeplitz_ || (cachedToeplitz_.value().rows() - 1) < maxLag) {
        if (!cachedAutocovariances_ || cachedAutocovariances_.value().empty() ||
            (cachedAutocovariances_.value().size() - 1) < maxLag) {
            cachedAutocovariances_ = analysis::stats::autocovariances(view_, maxLag);
        }
        cachedToeplitz_ = analysis::stats::toeplitz(cachedAutocovariances_.value(), maxLag);
    }
    return cachedToeplitz_.value();
}
double TimeSeriesAnalysis::zScore(double value) const {
    double mu = mean();
    double sigma = standardDeviation();
    if (sigma < 1e-9) return 0.0;
    return (value - mu) / sigma;
}

bool TimeSeriesAnalysis::isOutlier(double value, double threshold) const { return std::abs(zScore(value)) > threshold; }

void TimeSeriesAnalysis::invalidateCache() {
    cachedMean_.reset();
    cachedVarianceSample_.reset();
    cachedVariancePopulation_.reset();
    cachedKurtosis_.reset();
    cachedSkewness_.reset();
    cachedToeplitz_.reset();
    if (cachedACF_) cachedACF_.value().clear();
    cachedACF_.reset();
    if (cachedAutocovariances_) cachedAutocovariances_.value().clear();
    cachedAutocovariances_.reset();
}

}  // namespace analysis
