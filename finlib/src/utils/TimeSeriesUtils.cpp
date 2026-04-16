// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finlib/common/utils/TimeSeriesUtils.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace common::utils::timeSeries {

TimestampPtr makeRegularTimestamps(int64_t beginMs, int64_t endMs, int64_t frequencyMs) {
    if (frequencyMs <= 0) {
        throw std::invalid_argument("makeRegularTimestamps: frequencyMs must be positive.");
    }
    if (endMs < beginMs) {
        throw std::invalid_argument("makeRegularTimestamps: endMs must be >= beginMs.");
    }

    std::vector<int64_t> timestamps;
    const size_t expected = static_cast<size_t>((endMs - beginMs) / frequencyMs) + 1;
    timestamps.reserve(expected);
    for (int64_t t = beginMs; t <= endMs; t += frequencyMs) {
        timestamps.push_back(t);
    }
    return std::make_shared<const std::vector<int64_t>>(std::move(timestamps));
}

TimeSeries generateConstantTimeSeries(const std::string& id, int64_t beginMs, int64_t endMs, int64_t frequencyMs,
                                      double value) {
    TimestampPtr timestamps = makeRegularTimestamps(beginMs, endMs, frequencyMs);
    std::vector<double> values(timestamps->size(), value);
    return TimeSeries(id, std::move(timestamps), std::move(values));
}

TimeSeries generateConstantTimeSeries(const std::string& id, TimestampPtr timestamps, double value) {
    if (!timestamps) {
        throw std::invalid_argument("generateConstantTimeSeries: timestamps pointer is null.");
    }
    std::vector<double> values(timestamps->size(), value);
    return TimeSeries(id, std::move(timestamps), std::move(values));
}

}  // namespace common::utils::timeSeries
