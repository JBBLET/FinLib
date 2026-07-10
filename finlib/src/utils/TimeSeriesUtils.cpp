// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finlib/common/utils/TimeSeriesUtils.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "finlib/common/FinlibTypes.hpp"
#include "finlib/core/TimeSeries.hpp"

namespace ts::common::utils::timeSeries {

TimestampsPtr makeRegularTimestamps(Timestamp beginMs, Timestamp endMs, Timestamp frequencyMs) {
    if (frequencyMs <= 0) {
        throw std::invalid_argument("makeRegularTimestamps: frequencyMs must be positive.");
    }
    if (endMs < beginMs) {
        throw std::invalid_argument("makeRegularTimestamps: endMs must be >= beginMs.");
    }

    Timestamps timestamps;
    const size_t expected = static_cast<size_t>((endMs - beginMs) / frequencyMs) + 1;
    timestamps.reserve(expected);
    for (Timestamp t = beginMs; t <= endMs; t += frequencyMs) {
        timestamps.push_back(t);
    }
    return std::make_shared<const Timestamps>(std::move(timestamps));
}

TimeSeries generateConstantTimeSeries(const std::string& id, Timestamp beginMs, Timestamp endMs, Timestamp frequencyMs,
                                      double value) {
    TimestampsPtr timestamps = makeRegularTimestamps(beginMs, endMs, frequencyMs);
    std::vector<double> values(timestamps->size(), value);
    return TimeSeries(id, std::move(timestamps), std::move(values));
}

TimeSeries generateConstantTimeSeries(const std::string& id, TimestampsPtr timestamps, double value) {
    if (!timestamps) {
        throw std::invalid_argument("generateConstantTimeSeries: timestamps pointer is null.");
    }
    std::vector<double> values(timestamps->size(), value);
    return TimeSeries(id, std::move(timestamps), std::move(values));
}

TimeSeries generateStepSeries(const std::string& id, std::vector<std::pair<Timestamp, double>> breakpoints,
                              TimestampsPtr grid, double fillBefore) {
    if (!std::is_sorted(breakpoints.begin(),
                        breakpoints.end(),
                        [](const std::pair<Timestamp, double>& a, const std::pair<Timestamp, double>& b) {
                            return a.first < b.first;
                        })) {
        std::sort(breakpoints.begin(),
                  breakpoints.end(),
                  [](const std::pair<Timestamp, double>& a, const std::pair<Timestamp, double>& b) {
                      return a.first < b.first;
                  });
    }
    std::unordered_set<Timestamp> seen;
    auto newBegin =
        std::remove_if(breakpoints.rbegin(), breakpoints.rend(), [&seen](const std::pair<Timestamp, double>& tsPair) {
            return !seen.insert(tsPair.first).second;
        });
    breakpoints.erase(breakpoints.begin(), newBegin.base());

    std::vector<Timestamp> breakpointsTs(breakpoints.size() + 1);
    breakpointsTs.front() = grid->front();
    std::vector<double> values(breakpoints.size() + 1);
    values.front() = fillBefore;
    size_t count = 1;
    std::ranges::for_each(breakpoints, [&breakpointsTs, &values, &count](const auto& pair) {
        breakpointsTs[count] = pair.first;
        values[count] = pair.second;
        count++;
    });
    return TimeSeries{id, std::move(breakpointsTs), std::move(values)}.resampling(grid, InterpolationStrategy::Latest);
}
}  // namespace ts::common::utils::timeSeries
