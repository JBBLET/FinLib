// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once
#include <cstdint>
#include <string>

#include "finlib/core/TimeSeries.hpp"
struct LoaderCapabilities {
    int64_t earliestAvailableMS;
    int64_t finestFrequencyMs;
};

class ITimeSeriesLoader {
 public:
    virtual ~ITimeSeriesLoader() = default;

    virtual TimeSeries load(const std::string& id, int64_t startTimestampMs, int64_t endTimestampMs) const = 0;
    virtual LoaderCapabilities capabilities(const std::string& id) const = 0;
};
