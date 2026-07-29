// Copyright 2026 JBBLET
#pragma once

#include <cstddef>
#include <limits>
#include <vector>

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
