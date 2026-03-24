// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once
#include <cstdint>
#include <optional>
#include <vector>

#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/CoverageInfo.hpp"
#include "finlib/data/SeriesKey.hpp"
#include "finlib/data/interfaces/ITimeSeriesLoader.hpp"
#include "finlib/data/interfaces/ITimeSeriesSaver.hpp"

class ITimeSeriesRepository : public ITimeSeriesLoader, public ITimeSeriesSaver {
 public:
    virtual bool exists(const SeriesKey& key) const = 0;
    virtual std::optional<CoverageInfo> coverage(const SeriesKey& key) const = 0;

    virtual std::vector<int64_t> availableFrequencies(const std::string& id) const = 0;

    virtual TimeSeries load(const std::string& id, int64_t startTimestampMs, int64_t endTimestampMs) const override = 0;
    virtual TimeSeries load(const SeriesKey& key) const = 0;
    virtual TimeSeries load(const SeriesKey& key, int64_t startTimestampMs, int64_t endTimestampMs) const = 0;
};
