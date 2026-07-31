// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include "finlib/common/Error.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/SeriesKey.hpp"

namespace ts {

class ITimeSeriesSaver {
 public:
    ITimeSeriesSaver() = default;
    virtual ~ITimeSeriesSaver() = default;
    ITimeSeriesSaver(const ITimeSeriesSaver&) = default;
    ITimeSeriesSaver& operator=(const ITimeSeriesSaver&) = default;
    ITimeSeriesSaver(ITimeSeriesSaver&&) = default;
    ITimeSeriesSaver& operator=(ITimeSeriesSaver&&) = default;

    void save(const SeriesKey& key, const TimeSeries& ts) {
        ensure(!ts.isSynthetic(),
               "ITimeSeriesSaver::save: attempt to persist a synthetic (resampled) TimeSeries for series '{}'",
               key.SeriesId);
        doSave(key, ts);
    }

    void merge(const SeriesKey& key, const TimeSeries& ts) {
        ensure(!ts.isSynthetic(),
               "ITimeSeriesSaver::merge: attempt to persist a synthetic (resampled) TimeSeries for series '{}'",
               key.SeriesId);
        doMerge(key, ts);
    }

 protected:
    virtual void doSave(const SeriesKey& key, const TimeSeries& ts) = 0;
    virtual void doMerge(const SeriesKey& key, const TimeSeries& ts) = 0;
};
}  // namespace ts
