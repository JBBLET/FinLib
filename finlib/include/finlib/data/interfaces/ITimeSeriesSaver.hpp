// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <stdexcept>

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

    // Non-virtual public interface — guards against persisting synthetic (resampled) data.
    void save(const SeriesKey& key, const TimeSeries& ts, const CoverageInfo& cov) {
        if (ts.isSynthetic())
            throw std::logic_error("ITimeSeriesSaver::save: attempt to persist a synthetic (resampled) "
                                   "TimeSeries for series '" +
                                   key.SeriesId + "'");
        doSave(key, ts, cov);
    }

    void merge(const SeriesKey& key, const TimeSeries& ts) {
        if (ts.isSynthetic())
            throw std::logic_error("ITimeSeriesSaver::merge: attempt to persist a synthetic (resampled) "
                                   "TimeSeries for series '" +
                                   key.SeriesId + "'");
        doMerge(key, ts);
    }

 protected:
    virtual void doSave(const SeriesKey& key, const TimeSeries& ts, const CoverageInfo& cov) = 0;
    virtual void doMerge(const SeriesKey& key, const TimeSeries& ts) = 0;
};
