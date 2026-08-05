// Copyright 2026 JBBLET
#pragma once

#include <cstddef>
#include <format>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "finlib/common/Format.hpp"
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

    // Display
    std::string toString(const fmt::FormatSpec& spec = {}) const;
    void println(const fmt::FormatSpec& spec = {.mode = fmt::FormatMode::Describe}) const;
    void describe() const;

    const std::string& id() const noexcept { return id_; }
};
}  // namespace ts::simulation

template <>
struct std::formatter<ts::simulation::Distribution, char> {
    ts::fmt::FormatSpec spec;

    constexpr auto parse(std::format_parse_context& ctx) { return ts::fmt::parseFormatSpec(ctx, spec); }

    auto format(const ts::simulation::Distribution& distribution, std::format_context& ctx) const
        -> std::format_context::iterator {
        return std::format_to(ctx.out(), "{}", distribution.toString(spec));
    }
};
