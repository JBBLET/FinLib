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
#include "finlib/common/Format.hpp"
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

std::string Distribution::toString(const fmt::FormatSpec& spec) const {
    std::string identity = std::format("Distribution '{}' [n={}", id_, sorted_.size());
    if (!sorted_.empty()) {
        identity += std::format(", {} .. {}",
                                fmt::formatDouble(sorted_.front(), spec.precision),
                                fmt::formatDouble(sorted_.back(), spec.precision));
    }
    identity += ']';

    switch (spec.mode) {
        case fmt::FormatMode::Identity:
            return identity;
        case fmt::FormatMode::Head:
        case fmt::FormatMode::Tail:
        case fmt::FormatMode::Repr:
            // Sorted samples have no index of their own worth showing, so the row listing
            // reuses the series renderer with no timestamps.
            return fmt::renderSeries(identity, {}, sorted_, spec);
        default:
            break;
    }

    std::string out = identity;
    out += '\n';

    fmt::Table table({"statistic", "value"}, {fmt::Table::Align::Left, fmt::Table::Align::Right});
    table.addRow({"count", std::format("{}", sorted_.size())});
    if (sorted_.empty()) {
        out += table.render();
        return out;
    }

    // The old fixed .2f rendered a whole drawdown distribution as zeros — the scale here
    // has to come from the samples.
    const auto value = fmt::columnFormat(sorted_, spec.precision);

    table.addRow({"mean", value(mean())});
    // stddev() is the sample estimate, which throws on a single observation.
    if (sorted_.size() < 2) {
        table.addRow({"std", "N/A"});
        table.addRow({"variance", "N/A"});
    } else {
        const double sd = stddev();
        table.addRow({"std", value(sd)});
        // Squared units — the sample scale would round it away on a distribution of rates.
        table.addRow({"variance", fmt::formatDouble(sd * sd, spec.precision)});
    }
    table.addRule();
    table.addRow({"min", value(min())});
    table.addRow({"25% (Q1)", value(quantile(0.25))});
    table.addRow({"50% (median)", value(quantile(0.50))});
    table.addRow({"75% (Q3)", value(quantile(0.75))});
    table.addRow({"max", value(max())});
    table.addRow({"IQR (Q3-Q1)", value(quantile(0.75) - quantile(0.25))});

    out += table.render();
    return out;
}

void Distribution::println(const fmt::FormatSpec& spec) const { std::println("{}", toString(spec)); }

void Distribution::describe() const { println({.mode = fmt::FormatMode::Describe}); }

}  // namespace ts::simulation
