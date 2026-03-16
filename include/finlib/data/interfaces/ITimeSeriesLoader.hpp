#pragma once
#include <string>

#include "finlib/core/TimeSeries.hpp"

class ITimeSeriesLoader {
 public:
    virtual ~ITimeSeriesLoader() = default;

    virtual TimeSeries load(const std::string& id, int64_t start_ts, int64_t end_ts) = 0;
};
