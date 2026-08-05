// Copyright 2026 JBBLET
#pragma once

#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "finlib/common/Format.hpp"

namespace ts::simulation {

// Accumulators reduce a path to a handful of numbers *while it is being generated*, so the engine
// never has to hold paths x steps doubles in memory. Compose the ones you need inside your own path
// type, feed each new state to them from step(), and read them out in result().
//
// Storing every path instead costs paths * steps * 8 bytes: 10'000 x 10'000 is 800 MB. Reach for
// PathRecorder only for the handful of paths you actually intend to plot.
//
// All of these are plain structs on purpose — no virtuals, no allocation, fully inlinable inside the
// inner loop, and trivially copyable so one per path is free.

// Running mean and variance in a single pass, numerically stable (Welford).
struct Welford {
    std::size_t count = 0;
    double mean = 0.0;
    double m2 = 0.0;  // sum of squared deviations from the running mean

    void push(double x) {
        ++count;
        const double delta = x - mean;
        mean += delta / static_cast<double>(count);
        m2 += delta * (x - mean);
    }

    double populationVariance() const { return count == 0 ? 0.0 : m2 / static_cast<double>(count); }
    double sampleVariance() const { return count < 2 ? 0.0 : m2 / static_cast<double>(count - 1); }
};

// Peak-to-trough decline. Feed it the level (wealth, price), not the return.
struct DrawdownTracker {
    double peak = -std::numeric_limits<double>::infinity();
    double maxDrawdown = 0.0;  // fraction in [0, 1]
    double maxDrawdownAbsolute = 0.0;

    void push(double level) {
        if (level > peak) peak = level;
        const double decline = peak - level;
        if (decline > maxDrawdownAbsolute) maxDrawdownAbsolute = decline;
        if (peak > 0.0) {
            const double relative = decline / peak;
            if (relative > maxDrawdown) maxDrawdown = relative;
        }
    }
};

struct RunningExtrema {
    double min = std::numeric_limits<double>::infinity();
    double max = -std::numeric_limits<double>::infinity();

    void push(double x) {
        if (x < min) min = x;
        if (x > max) max = x;
    }
};

// Did the path ever cross a barrier, and when? Ruin probability, knock-in/knock-out, target dates.
struct ThresholdCrossing {
    double threshold = 0.0;
    bool fromAbove = true;  // true: trigger on level <= threshold; false: on level >= threshold
    bool crossed = false;
    std::size_t firstCrossingStep = 0;

    void push(std::size_t step, double level) {
        if (crossed) return;
        if (fromAbove ? (level <= threshold) : (level >= threshold)) {
            crossed = true;
            firstCrossingStep = step;
        }
    }
};

// Keeps the whole trajectory. Opt-in — see the memory note above.
struct PathRecorder {
    std::vector<double> levels;

    explicit PathRecorder(std::size_t expectedSteps = 0) { levels.reserve(expectedSteps + 1); }
    void push(double level) { levels.push_back(level); }
};
}  // namespace ts::simulation

// The formatters below live outside the structs, so the accumulators stay plain aggregates:
// no virtuals, no members added, still trivially copyable and still free to keep one per path.
//
// Every one of them has a "never pushed" state that is not zero — the extrema seed to
// infinities and the drawdown peak to -inf — so each reports that state rather than printing
// `inf` and leaving the reader to work out whether a path really ran.

template <>
struct std::formatter<ts::simulation::Welford> : std::formatter<std::string_view> {
    auto format(const ts::simulation::Welford& welford, std::format_context& ctx) const
        -> std::format_context::iterator {
        std::string rendered;
        if (welford.count == 0) {
            rendered = "Welford[empty]";
        } else {
            // The sample variance needs two observations; the running mean does not.
            rendered = std::format("Welford[n={}, mean={}, sd={}]",
                                   welford.count,
                                   ts::fmt::formatDouble(welford.mean),
                                   welford.count < 2 ? std::string{"N/A"}
                                                     : ts::fmt::formatDouble(std::sqrt(welford.sampleVariance())));
        }
        return std::formatter<std::string_view>::format(rendered, ctx);
    }
};

template <>
struct std::formatter<ts::simulation::DrawdownTracker> : std::formatter<std::string_view> {
    auto format(const ts::simulation::DrawdownTracker& drawdown, std::format_context& ctx) const
        -> std::format_context::iterator {
        std::string rendered;
        if (!std::isfinite(drawdown.peak)) {
            rendered = "Drawdown[empty]";
        } else {
            // maxDrawdown is a fraction of the running peak, which is only ever read as a
            // percentage — printing 0.5106 invites it to be misread as an absolute loss.
            rendered = std::format("Drawdown[max={}%, absolute={}, peak={}]",
                                   ts::fmt::formatDouble(drawdown.maxDrawdown * 100.0, 2),
                                   ts::fmt::formatDouble(drawdown.maxDrawdownAbsolute),
                                   ts::fmt::formatDouble(drawdown.peak));
        }
        return std::formatter<std::string_view>::format(rendered, ctx);
    }
};

template <>
struct std::formatter<ts::simulation::RunningExtrema> : std::formatter<std::string_view> {
    auto format(const ts::simulation::RunningExtrema& extrema, std::format_context& ctx) const
        -> std::format_context::iterator {
        // Seeded inverted, so min > max is exactly the never-pushed state.
        const std::string rendered =
            extrema.min > extrema.max ? std::string{"Extrema[empty]"}
                                      : std::format("Extrema[min={}, max={}]",
                                                    ts::fmt::formatDouble(extrema.min),
                                                    ts::fmt::formatDouble(extrema.max));
        return std::formatter<std::string_view>::format(rendered, ctx);
    }
};

template <>
struct std::formatter<ts::simulation::ThresholdCrossing> : std::formatter<std::string_view> {
    auto format(const ts::simulation::ThresholdCrossing& crossing, std::format_context& ctx) const
        -> std::format_context::iterator {
        const std::string_view direction = crossing.fromAbove ? "from above" : "from below";
        const std::string rendered =
            crossing.crossed
                ? std::format("ThresholdCrossing[crossed at step {}, threshold={} {}]",
                              crossing.firstCrossingStep,
                              ts::fmt::formatDouble(crossing.threshold),
                              direction)
                : std::format("ThresholdCrossing[not crossed, threshold={} {}]",
                              ts::fmt::formatDouble(crossing.threshold),
                              direction);
        return std::formatter<std::string_view>::format(rendered, ctx);
    }
};

// The one accumulator that holds a series rather than a summary, so it takes the full
// grammar: {} for the identity, {:h}/{:t}/{:r} to look at the trajectory itself.
template <>
struct std::formatter<ts::simulation::PathRecorder> {
    ts::fmt::FormatSpec spec;

    constexpr auto parse(std::format_parse_context& ctx) { return ts::fmt::parseFormatSpec(ctx, spec); }

    auto format(const ts::simulation::PathRecorder& recorder, std::format_context& ctx) const
        -> std::format_context::iterator {
        const std::string identity = std::format("PathRecorder[n={}]", recorder.levels.size());
        if (spec.mode == ts::fmt::FormatMode::Identity) return std::format_to(ctx.out(), "{}", identity);
        if (spec.mode == ts::fmt::FormatMode::Describe) {
            return std::format_to(ctx.out(), "{}", ts::fmt::renderDescribe(identity, recorder.levels, spec.precision));
        }
        // A recorded path has no timestamps of its own — steps are its index.
        return std::format_to(ctx.out(), "{}", ts::fmt::renderSeries(identity, {}, recorder.levels, spec));
    }
};
