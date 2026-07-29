// Copyright 2026 JBBLET
#include "finlib/core/Resampling.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <future>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "finlib/common/FinlibTypes.hpp"
#include "finlib/core/TimeSeries.hpp"

namespace ts {
namespace {

constexpr std::size_t kChunkSize = 20'000;

constexpr bool needsRandomness(InterpolationStrategy s) { return s == InterpolationStrategy::Stochastic; }

struct BridgeNoise {
    Rng rng;
    std::normal_distribution<double> gauss{0.0, 1.0};
    explicit BridgeNoise(Rng r) : rng(std::move(r)) {}
    double operator()() { return gauss(rng); }
};

double bridgeNoiseAt(Timestamp target, Timestamp t1, Timestamp t2, double varianceRate, BridgeNoise& noise) {
    const double bridgeTicks =
        static_cast<double>(target - t1) * static_cast<double>(t2 - target) / static_cast<double>(t2 - t1);
    return noise() * std::sqrt(varianceRate * bridgeTicks);
}

double applyStrategy(InterpolationStrategy strategy, BridgeNoise* noise, double varianceRate, Timestamp target,
                     Timestamp t1, double v1, Timestamp t2, double v2) {
    const double fraction = static_cast<double>(target - t1) / static_cast<double>(t2 - t1);
    const double linearVal = v1 + fraction * (v2 - v1);
    switch (strategy) {
        case InterpolationStrategy::Linear:
            return linearVal;
        case InterpolationStrategy::Stochastic:
            return noise ? linearVal + bridgeNoiseAt(target, t1, t2, varianceRate, *noise) : linearVal;
        case InterpolationStrategy::Nearest:
            return (target - t1 < t2 - target) ? v1 : v2;
        case InterpolationStrategy::Latest:
            return v1;
        case InterpolationStrategy::Exact:
            return v1;  // handled before this call
    }
    return v1;
}

std::vector<double> partialWalk(const TimeSeries& src, const Timestamps& target, std::size_t startIndex,
                                std::size_t endIndex, InterpolationStrategy strategy, BridgeNoise* noise,
                                double varianceRate) {
    const std::size_t chunkLength = endIndex - startIndex;
    std::vector<double> newValues(chunkLength);

    // Use the span view so tsOffset_ is applied — the shared TimestampsPtr can be longer than
    // values() when the series came from a slice or an arithmetic operator.
    const auto span = src.getTimestamps();
    const auto& values = src.getValues();
    const std::size_t originalSize = values.size();

    auto it = std::lower_bound(span.begin(), span.end(), target[startIndex]);
    std::size_t dataIndex = static_cast<std::size_t>(std::distance(span.begin(), it));
    if (dataIndex > 0) --dataIndex;

    for (std::size_t i = 0; i < chunkLength; ++i) {
        const Timestamp currentTarget = target[startIndex + i];
        while (dataIndex < originalSize - 1 && span[dataIndex + 1] <= currentTarget) ++dataIndex;

        if (strategy == InterpolationStrategy::Exact) {
            newValues[i] =
                (span[dataIndex] == currentTarget) ? values[dataIndex] : std::numeric_limits<double>::quiet_NaN();
            continue;
        }
        if (currentTarget <= span[0]) {
            newValues[i] = values[0];
        } else if (dataIndex >= originalSize - 1) {
            newValues[i] = values.back();
        } else {
            newValues[i] = applyStrategy(strategy,
                                         noise,
                                         varianceRate,
                                         currentTarget,
                                         span[dataIndex],
                                         values[dataIndex],
                                         span[dataIndex + 1],
                                         values[dataIndex + 1]);
        }
    }
    return newValues;
}

std::vector<double> resampleValues(const TimeSeries& src, const Timestamps& target, InterpolationStrategy strategy,
                                   const StochasticParams& params) {
    if (!std::is_sorted(target.begin(), target.end()))
        throw std::invalid_argument("target_timestamps must be sorted for resampling.");
    if (src.getValues().empty())
        throw std::runtime_error("resample: cannot resample from empty series '" + src.getId() + "'");

    const std::size_t n = target.size();
    if (n == 0) return {};

    const bool random = needsRandomness(strategy);
    double varianceRate = 0.0;
    if (random) varianceRate = params.varianceRate ? *params.varianceRate : varianceRatePerTick(src);

    const std::size_t chunks = (n + kChunkSize - 1) / kChunkSize;
    std::vector<double> out(n);

    auto runChunk = [&](std::size_t c) {
        const std::size_t start = c * kChunkSize;
        const std::size_t end = std::min(start + kChunkSize, n);
        std::optional<BridgeNoise> noise;
        if (random) noise.emplace(rngForStream(params.seed, RngDomain::Resampling, c));  // stream = chunk ordinal
        auto part = partialWalk(src, target, start, end, strategy, noise ? &*noise : nullptr, varianceRate);
        std::copy(part.begin(), part.end(), out.begin() + static_cast<std::ptrdiff_t>(start));
    };

    if (chunks == 1) {
        runChunk(0);
    } else {
        const unsigned hw = std::thread::hardware_concurrency();
        const std::size_t width = (hw == 0) ? 1 : hw;
        for (std::size_t c0 = 0; c0 < chunks; c0 += width) {  // waves keep thread count bounded
            std::vector<std::future<void>> futs;
            for (std::size_t c = c0; c < std::min(c0 + width, chunks); ++c)
                futs.push_back(std::async(std::launch::async, [&, c] { runChunk(c); }));
            for (auto& f : futs) f.get();
        }
    }
    return out;
}
}  // namespace

double varianceRatePerTick(const TimeSeries& src) {
    const auto& values = src.getValues();
    if (values.size() < 2) return 0.0;
    double qv = 0.0;
    for (std::size_t i = 1; i < values.size(); ++i) {
        const double dv = values[i] - values[i - 1];
        qv += dv * dv;
    }
    const auto span = src.getTimestamps();
    const double totalTicks = static_cast<double>(span.back() - span.front());
    return totalTicks > 0.0 ? qv / totalTicks : 0.0;
}

TimeSeries resample(const TimeSeries& src, TimestampsPtr target, InterpolationStrategy strategy,
                    const StochasticParams& params) {
    if (!target) throw std::invalid_argument("targetTimestamps pointer is null.");
    auto values = resampleValues(src, *target, strategy, params);  // must precede the move below
    return TimeSeries::synthetic("Resampled " + src.getId(), std::move(target), std::move(values));
}

TimeSeries resample(const TimeSeries& src, const Timestamps& target, InterpolationStrategy strategy,
                    const StochasticParams& params) {
    return resample(src, std::make_shared<const Timestamps>(target), strategy, params);
}
}  // namespace ts
