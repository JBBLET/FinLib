// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once
#include <optional>

#include "finlib/common/FinlibTypes.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/CoverageInfo.hpp"
#include "finlib/data/SeriesKey.hpp"
#include "finlib/data/interfaces/ITimeSeriesLoader.hpp"
#include "finlib/data/interfaces/ITimeSeriesSaver.hpp"

class ITimeSeriesRepository : public ITimeSeriesLoader, public ITimeSeriesSaver {
 public:
    virtual bool exists(const SeriesKey& key) const = 0;
    virtual std::optional<CoverageInfo> coverage(const SeriesKey& key) const = 0;

    virtual Timestamps availableFrequencies(const std::string& id) const = 0;

    TimeSeries load(const std::string& id, Timestamp startTimestampMs, Timestamp endTimestampMs) const override = 0;
    virtual TimeSeries load(const SeriesKey& key) const = 0;
    virtual TimeSeries load(const SeriesKey& key, Timestamp startTimestampMs, Timestamp endTimestampMs) const = 0;
};
