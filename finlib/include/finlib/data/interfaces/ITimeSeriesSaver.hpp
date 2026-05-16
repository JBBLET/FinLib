// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/CoverageInfo.hpp"
#include "finlib/data/SeriesKey.hpp"

class ITimeSeriesSaver {
 public:
    ITimeSeriesSaver() = default;
    virtual ~ITimeSeriesSaver() = default;
    ITimeSeriesSaver(const ITimeSeriesSaver&) = default;
    ITimeSeriesSaver& operator=(const ITimeSeriesSaver&) = default;
    ITimeSeriesSaver(ITimeSeriesSaver&&) = default;
    ITimeSeriesSaver& operator=(ITimeSeriesSaver&&) = default;

    virtual void save(const SeriesKey& key, const TimeSeries& TimeSeries, const CoverageInfo& coverageInfo) = 0;
    virtual void merge(const SeriesKey& key, const TimeSeries& newTimeSeries) = 0;
};
