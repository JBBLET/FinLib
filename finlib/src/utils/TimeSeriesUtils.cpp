// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finlib/common/utils/TimeSeriesUtils.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "finlib/common/FinlibTypes.hpp"

namespace common::utils::timeSeries {

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

}  // namespace common::utils::timeSeries
