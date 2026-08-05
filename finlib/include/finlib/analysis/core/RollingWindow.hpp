// Copyright 2026 JBBLET

#pragma once
#include <algorithm>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Eigen/Core"
#include "finlib/common/Format.hpp"

namespace ts::simulation {

class RollingWindow {
    Eigen::VectorXd w_;

 public:
    RollingWindow() = default;
    explicit RollingWindow(const std::vector<double>& seed)
        : w_(Eigen::Map<const Eigen::VectorXd>(seed.data(), static_cast<Eigen::Index>(seed.size()))) {}

    const Eigen::VectorXd& get() const { return w_; }
    Eigen::Index size() const { return w_.size(); }

    void push(double x) {
        const auto n = w_.size();
        if (n == 0) return;
        if (n > 1) w_.head(n - 1) = w_.tail(n - 1).eval();  // .eval(): segments alias
        w_(n - 1) = x;
    }
};
}  // namespace ts::simulation

// A model's context window is small by construction, so it reads better inline than as the
// table the generic Eigen formatter would produce. Oldest entry first, matching push order.
// `{:h4}` caps how many are listed.
template <>
struct std::formatter<ts::simulation::RollingWindow> {
    ts::fmt::FormatSpec spec;

    constexpr auto parse(std::format_parse_context& ctx) { return ts::fmt::parseFormatSpec(ctx, spec); }

    auto format(const ts::simulation::RollingWindow& window, std::format_context& ctx) const
        -> std::format_context::iterator {
        const auto& values = window.get();
        const auto n = static_cast<std::size_t>(values.size());
        if (n == 0) return std::format_to(ctx.out(), "RollingWindow[empty]");

        const std::span<const double> samples{values.data(), n};
        const std::size_t shown = spec.mode == ts::fmt::FormatMode::Identity ? n : std::min(spec.count, n);

        // Per-value precision rather than a shared column width: this is an inline list, so
        // there is no column to line up and the padding would only be noise.
        std::string body;
        for (std::size_t i = 0; i < shown; ++i) {
            if (i != 0) body += ", ";
            body += ts::fmt::formatDouble(samples[i], spec.precision);
        }
        if (shown < n) body += ", ...";
        return std::format_to(ctx.out(), "RollingWindow[n={}: {}]", n, body);
    }
};
