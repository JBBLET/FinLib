// Copyright 2026 JBBLET
#pragma once

#include <cstddef>
#include <limits>
#include <vector>

#include "finlib/analysis/MetricHandle.hpp"
#include "finlib/core/TimeSeriesView.hpp"

namespace ts::analysis::metrics {

inline MetricFn<std::vector<double>> movingAverage(size_t window) {
    return [window](TimeSeriesView v) -> std::vector<double> {
        const size_t n = v.size();
        std::vector<double> result(n, std::numeric_limits<double>::quiet_NaN());
        double sum = 0.0;
        for (size_t i = 0; i < n; ++i) {
            sum += v[i];
            if (i >= window) sum -= v[i - window];
            if (i + 1 >= window) result[i] = sum / static_cast<double>(window);
        }
        return result;
    };
}

}  // namespace ts::analysis::metrics
