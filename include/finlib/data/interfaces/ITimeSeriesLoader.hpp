// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once
#include <string>

#include "finlib/core/TimeSeries.hpp"

class ITimeSeriesLoader {
 public:
    virtual ~ITimeSeriesLoader() = default;

    virtual TimeSeries load(const std::string& id, int64_t startTimestamp, int64_t endTimestamp) = 0;
};
