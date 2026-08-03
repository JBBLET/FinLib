// Copyright 2026 JBBLET

#include "finlib/analysis/simulation/monteCarlo/Distribution.hpp"

#include <matplot/matplot.h>

#include <algorithm>
#include <format>
#include <iterator>
#include <print>
#include <string>
#include <utility>
#include <vector>

#include "finlib/common/Error.hpp"
#include "finlib/common/Log.hpp"
#include "finlib/core/StatsCore.hpp"
#include "matplot/axes_objects/histogram.h"
#include "matplot/freestanding/axes_functions.h"

namespace ts::simulation {

namespace stats = ts::analysis::stats;

Distribution::Distribution(std::string id, std::vector<double> v) : id_{std::move(id)}, sorted_{std::move(v)} {
    if (!std::is_sorted(sorted_.begin(), sorted_.end())) std::sort(sorted_.begin(), sorted_.end());
}

double Distribution::quantile(double q) const {
    ensure(!sorted_.empty(), "Quantile of an empty distribution");
    ensure(q >= 0.0 && q <= 1.0, "Quantile value {} invalid", q);
    return stats::quantileSorted(sorted_, q);
}

double Distribution::mean() const { return stats::mean(sorted_); }

double Distribution::stddev() const { return stats::standardDeviation(sorted_, stats::VarianceType::Sample); }

double Distribution::min() const {
    ensure(!sorted_.empty(), "Minimum of an empty distribution");
    return sorted_.front();
}

double Distribution::max() const {
    ensure(!sorted_.empty(), "Maximum of an empty distribution");
    return sorted_.back();
}

double Distribution::cdf(double x) const {
    if (sorted_.empty()) return 0.0;
    const auto it = std::upper_bound(sorted_.begin(), sorted_.end(), x);
    return static_cast<double>(std::distance(sorted_.begin(), it)) / static_cast<double>(sorted_.size());
}

void Distribution::plot() const {
    auto f = matplot::figure(true);
    auto a = matplot::histogram::binning_algorithm::automatic;
    logging::info("Distribution Plot {}", id_);
    matplot::subplot(2, 1, 1);
    auto h = matplot::hist(sorted_, a, matplot::histogram::normalization::count);
    f->draw();
    matplot::title("Histogram of {} Distribution", id_);
    matplot::subplot(2, 1, 2);
    auto cdfPlot = matplot::hist(sorted_, a, matplot::histogram::normalization::cdf);
    matplot::title("Cumulative Distribution Function (CDF) of {}", id_);
    f->draw();
    matplot::show();
}

void Distribution::print() const {
    logging::info("Distribution of {}", id_);
    struct StatRow {
        std::string name;
        double value;
    };
    std::vector<StatRow> stats = {
        {.name = "Count", .value = static_cast<double>(sorted_.size())},
        {.name = "Minimum", .value = min()},
        {.name = "Maximum", .value = max()},
        {.name = "Mean", .value = mean()},
        {.name = "Variance", .value = stddev() * stddev()},
        {.name = "Std Dev", .value = stddev()},
    };
    std::vector<StatRow> quantiles = {
        {.name = "1st Quartile (Q1)", .value = quantile(0.25)},
        {.name = "Median (Q2)", .value = quantile(0.5)},
        {.name = "3rd Quartile (Q3)", .value = quantile(0.75)},
        {.name = "Interquartile range (Q3-Q1)", .value = quantile(0.75) - quantile(0.25)},
    };

    std::println("{:<20} | {:>10}", "Statistic", "Value");
    std::println("{:-<20}-+-{:-<10}", "", "");
    for (const auto& [name, value] : stats) {
        std::println("{:<20} | {:>10.2f}", name, value);
    }
    std::println("{:-<20}-+-{:-<10}", "", "");
    std::println("");
    std::println("{:<20} | {:>10}", "Statistic", "Value");
    std::println("{:-<20}-+-{:-<10}", "", "");
    for (const auto& [name, value] : quantiles) {
        std::println("{:<20} | {:>10.2f}", name, value);
    }
    std::println("{:-<20}-+-{:-<10}", "", "");
}  // namespace ts::simulation

}  // namespace ts::simulation
