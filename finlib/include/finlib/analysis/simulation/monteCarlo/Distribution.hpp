// Copyright 2026 JBBLET
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "finlib/core/StatsCore.hpp"

namespace ts::simulation {

class Distribution {
    std::vector<double> sorted_;
    std::string id_;

 public:
    explicit Distribution(std::string id, std::vector<double> v);

    template <class R, class Proj>
    static Distribution from(std::string id, const std::vector<R>& rs, Proj proj) {
        std::vector<double> v;
        v.reserve(rs.size());
        for (const auto& r : rs) v.push_back(static_cast<double>(std::invoke(proj, r)));
        return Distribution(std::move(id), std::move(v));
    }

    std::size_t size() const noexcept { return sorted_.size(); }
    bool empty() const noexcept { return sorted_.empty(); }
    analysis::stats::Samples samples() const noexcept { return sorted_; }

    double quantile(double q) const;
    double mean() const;
    double stddev() const;
    double min() const;
    double max() const;

    double cdf(double x) const;

    void plot() const;
    void print() const;
};
}  // namespace ts::simulation
