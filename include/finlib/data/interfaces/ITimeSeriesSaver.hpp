// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once
#include <string>

#include "finlib/core/TimeSeries.hpp"

class ITimeSeriesSaver {
 public:
    virtual ~ITimeSeriesSaver() = default;

    virtual void save(const std::string& name, const TimeSeries& TimeSeries) = 0;
};
