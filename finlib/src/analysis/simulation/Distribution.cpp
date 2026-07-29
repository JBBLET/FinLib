// Copyright 2026 JBBLET

#include "finlib/analysis/simulation/monteCarlo/Distribution.hpp"

#include <algorithm>
#include <format>
#include <iterator>
#include <utility>
#include <vector>

#include "finlib/common/Exception.hpp"
#include "finlib/core/StatsCore.hpp"

namespace ts::simulation {

namespace stats = ts::analysis::stats;

Distribution::Distribution(std::vector<double> v) : sorted_{std::move(v)} {
    if (!std::is_sorted(sorted_.begin(), sorted_.end())) std::sort(sorted_.begin(), sorted_.end());
}

double Distribution::quantile(double q) const {
    if (sorted_.empty()) throw Exception("Quantile of an empty distribution");
    if (!(q >= 0.0 && q <= 1.0)) throw Exception(std::format("Quantile value {} invalid", q));
    return stats::quantileSorted(sorted_, q);
}

double Distribution::mean() const { return stats::mean(sorted_); }

double Distribution::stddev() const { return stats::standardDeviation(sorted_, stats::VarianceType::Sample); }

double Distribution::min() const {
    if (sorted_.empty()) throw Exception("Minimum of an empty distribution");
    return sorted_.front();
}

double Distribution::max() const {
    if (sorted_.empty()) throw Exception("Maximum of an empty distribution");
    return sorted_.back();
}

double Distribution::cdf(double x) const {
    if (sorted_.empty()) return 0.0;
    const auto it = std::upper_bound(sorted_.begin(), sorted_.end(), x);
    return static_cast<double>(std::distance(sorted_.begin(), it)) / static_cast<double>(sorted_.size());
}
}  // namespace ts::simulation
