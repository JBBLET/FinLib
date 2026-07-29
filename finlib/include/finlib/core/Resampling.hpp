// Copyright 2026 JBBLET
#pragma once
#include <optional>

#include "finlib/common/Random.hpp"
#include "finlib/core/TimeSeries.hpp"

namespace ts {

enum class InterpolationStrategy { Linear, Stochastic, Nearest, Exact, Latest };

struct StochasticParams {
    std::optional<double> varianceRate = 0.0;  // variance per timestamp tick; 0 degenerates to Linear
    Seed seed = kDefaultSeed;
};

// Realized quadratic variation per tick — the default bridge scale when none is supplied.
double varianceRatePerTick(const TimeSeries& src);

TimeSeries resample(const TimeSeries& src, TimestampsPtr target, InterpolationStrategy strategy,
                    const StochasticParams& params = {});
TimeSeries resample(const TimeSeries& src, const Timestamps& target, InterpolationStrategy strategy,
                    const StochasticParams& params = {});
}  // namespace ts
