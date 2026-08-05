// Copyright 2026 JBBLET

#include "finlib/analysis/seriesAnalysis/TimeSeriesAnalysis.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <vector>

#include "finlib/common/Error.hpp"
#include "finlib/common/Format.hpp"
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
        throw InvalidArgument("Invalid Variance type");
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

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

std::string TimeSeriesAnalysis::toString(const fmt::FormatSpec& spec) const {
    const std::string identity = std::format("TimeSeriesAnalysis of {:s}", view_);
    if (spec.mode != fmt::FormatMode::Describe && spec.mode != fmt::FormatMode::Repr) return identity;

    const size_t n = view_.size();
    std::string out = identity;
    out += '\n';

    fmt::Table table({"statistic", "value"}, {fmt::Table::Align::Left, fmt::Table::Align::Right});
    table.addRow({"count", std::format("{}", n)});
    if (n == 0) {
        out += table.render();
        return out;
    }

    // Three scales share this table and must not share a precision. Location and spread
    // carry the units of the data; the variances are those units squared, so quoting them
    // at the data's precision collapses sample and population onto the same digits; and
    // the shape statistics are dimensionless and O(1).
    const std::span<const double> samples{view_.begin(), n};
    const auto value = fmt::columnFormat(samples, spec.precision);
    auto cell = [&](const std::optional<double>& v) { return v ? value(*v) : std::string{"N/A"}; };
    auto squared = [&](const std::optional<double>& v) { return fmt::naOr(v, spec.precision); };
    auto shape = [&](const std::optional<double>& v) { return fmt::naOr(v, spec.precision >= 0 ? spec.precision : 4); };

    // Non-finite entries are excluded from the extrema for the same reason the stats
    // functions skip them: one NaN would otherwise decide the whole comparison.
    std::optional<double> minimum;
    std::optional<double> maximum;
    size_t nonFinite = 0;
    for (double v : samples) {
        if (!std::isfinite(v)) {
            ++nonFinite;
            continue;
        }
        minimum = minimum ? std::min(*minimum, v) : v;
        maximum = maximum ? std::max(*maximum, v) : v;
    }
    if (nonFinite != 0) table.addRow({"non-finite", std::format("{}", nonFinite)});

    table.addRow({"mean", value(mean())});
    // standardDeviation() is derived from the population variance — labelled as it is computed.
    table.addRow({"std (population)", cell(standardDeviation())});
    table.addRow({"var (sample)", squared(variance(stats::VarianceType::Sample))});
    table.addRow({"var (population)", squared(variance(stats::VarianceType::Population))});
    table.addRow({"skewness", shape(skewness())});
    table.addRow({"kurtosis", shape(kurtosis())});
    table.addRow({"min", cell(minimum)});
    table.addRow({"max", cell(maximum)});

    // Lag 0 is 1 by construction and carries no information, so the table starts at lag 1.
    // acf() returns fewer coefficients than asked for on a short window rather than throwing.
    const size_t requestedLags = spec.count == 0 ? 5 : spec.count;
    const auto& coefficients = acf(requestedLags);
    if (coefficients.size() > 1) {
        table.addRule();
        for (size_t lag = 1; lag < coefficients.size(); ++lag) {
            table.addRow({std::format("acf[{}]", lag), fmt::formatDouble(coefficients[lag], 4)});
        }
    }

    out += table.render();
    return out;
}

void TimeSeriesAnalysis::println(const fmt::FormatSpec& spec) const { std::println("{}", toString(spec)); }

void TimeSeriesAnalysis::describe(size_t autocorrelationLags) const {
    println({.mode = fmt::FormatMode::Describe, .count = autocorrelationLags});
}

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
