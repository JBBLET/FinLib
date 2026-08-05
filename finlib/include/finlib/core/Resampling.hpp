// Copyright 2026 JBBLET
#pragma once
#include <format>
#include <optional>
#include <string_view>

#include "finlib/common/Random.hpp"
#include "finlib/core/TimeSeries.hpp"

namespace ts {

enum class InterpolationStrategy { Linear, Stochastic, Nearest, Exact, Latest };

// Scoped enums are not formattable by default, so until now no diagnostic could name the
// strategy it was given. The free function is useful on its own (CSV round-trips, UI labels).
constexpr std::string_view toString(InterpolationStrategy strategy) {
    switch (strategy) {
        case InterpolationStrategy::Linear: return "Linear";
        case InterpolationStrategy::Stochastic: return "Stochastic";
        case InterpolationStrategy::Nearest: return "Nearest";
        case InterpolationStrategy::Exact: return "Exact";
        case InterpolationStrategy::Latest: return "Latest";
    }
    return "<unknown InterpolationStrategy>";
}

struct StochasticParams {
    std::optional<double> varianceRate = std::nullopt;
    Seed seed = kDefaultSeed;
};

double varianceRatePerTick(const TimeSeries& src);

TimeSeries resample(const TimeSeries& src, TimestampsPtr target, InterpolationStrategy strategy,
                    const StochasticParams& params = {});
TimeSeries resample(const TimeSeries& src, const Timestamps& target, InterpolationStrategy strategy,
                    const StochasticParams& params = {});
}  // namespace ts

template <>
struct std::formatter<ts::InterpolationStrategy> : std::formatter<std::string_view> {
    auto format(ts::InterpolationStrategy strategy, std::format_context& ctx) const -> std::format_context::iterator {
        return std::formatter<std::string_view>::format(ts::toString(strategy), ctx);
    }
};
