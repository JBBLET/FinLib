// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <string>

#include "finlib/common/FinlibTypes.hpp"
#include "finlib/core/TimeSeries.hpp"

struct LoaderCapabilities {
    Timestamp earliestAvailableMS;
    Timestamp finestFrequencyMs;
};

class ITimeSeriesLoader {
 public:
    ITimeSeriesLoader() = default;
    virtual ~ITimeSeriesLoader() = default;
    ITimeSeriesLoader(const ITimeSeriesLoader&) = default;
    ITimeSeriesLoader& operator=(const ITimeSeriesLoader&) = default;
    ITimeSeriesLoader(ITimeSeriesLoader&&) = default;
    ITimeSeriesLoader& operator=(ITimeSeriesLoader&&) = default;

    virtual TimeSeries load(const std::string& id, Timestamp startTimestampMs, Timestamp endTimestampMs) const = 0;
    virtual LoaderCapabilities capabilities(const std::string& id) const = 0;
};
