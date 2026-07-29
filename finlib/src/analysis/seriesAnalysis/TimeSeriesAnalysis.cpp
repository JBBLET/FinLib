// Copyright 2026 JBBLET

#include "finlib/analysis/seriesAnalysis/TimeSeriesAnalysis.hpp"

#include <cmath>
#include <stdexcept>
#include <vector>

#include "finlib/core/StatsCore.hpp"

namespace ts::analysis {

double TimeSeriesAnalysis::mean() const {
    if (!cachedMean_) {
        cachedMean_ = stats::mean(view_);
    }
    return cachedMean_.value();
}

std::optional<double> TimeSeriesAnalysis::variance(stats::VarianceType type) const {
    std::optional<double>* cache = nullptr;
    if (type == stats::VarianceType::Sample) {
        cache = &cachedVarianceSample_;
    } else if (type == stats::VarianceType::Population) {
        cache = &cachedVariancePopulation_;
    } else {
        throw std::invalid_argument("Invalid Variance type");
    }

    if (!*cache) {
        try {
            double v = ts::analysis::stats::varianceFast(view_, type);
            if (!std::isfinite(v)) return std::nullopt;
            *cache = v;
        } catch (const std::exception&) {
            // Too few observations to define the variance.
            return std::nullopt;
        }
    }
    return *cache;
}

std::optional<double> TimeSeriesAnalysis::standardDeviation() const {
    auto var = variance(stats::VarianceType::Population);
    if (!var) return std::nullopt;
    double sd = std::sqrt(*var);
    if (!std::isfinite(sd)) return std::nullopt;
    return sd;
}

std::optional<double> TimeSeriesAnalysis::skewness() const {
    if (!cachedSkewness_) {
        try {
            double v = ts::analysis::stats::skewness(view_);
            if (!std::isfinite(v)) return std::nullopt;
            cachedSkewness_ = v;
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }
    return cachedSkewness_;
}

std::optional<double> TimeSeriesAnalysis::kurtosis() const {
    if (!cachedKurtosis_) {
        try {
            double v = ts::analysis::stats::kurtosis(view_);
            if (!std::isfinite(v)) return std::nullopt;
            cachedKurtosis_ = v;
        } catch (const std::exception&) {
            // Fewer than four observations — kurtosis is undefined.
            return std::nullopt;
        }
    }
    return cachedKurtosis_;
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
        cachedAutocovariances_ = ts::analysis::stats::autocovariances(view_, maxLag);
    }
    return cachedAutocovariances_.value();
}

Eigen::MatrixXd TimeSeriesAnalysis::toeplitz(size_t maxLag) {
    if (!cachedToeplitz_ || (cachedToeplitz_.value().rows() - 1) < maxLag) {
        if (!cachedAutocovariances_ || cachedAutocovariances_.value().empty() ||
            (cachedAutocovariances_.value().size() - 1) < maxLag) {
            cachedAutocovariances_ = ts::analysis::stats::autocovariances(view_, maxLag);
        }
        cachedToeplitz_ = ts::analysis::stats::toeplitzFromAutocovariances(cachedAutocovariances_.value(), maxLag);
    }
    return cachedToeplitz_.value();
}
double TimeSeriesAnalysis::zScore(double value) const {
    double mu = mean();
    auto sigma = standardDeviation();
    if (!sigma || *sigma < 1e-9) return 0.0;
    return (value - mu) / *sigma;
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

}  // namespace ts::analysis
