// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "finlib/common/FinlibTypes.hpp"
#include "finlib/core/Resampling.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/SeriesKey.hpp"
#include "finlib/data/TimeRange.hpp"
#include "finlib/data/implementation/CachedTimeSeriesRepository.hpp"
#include "finlib/data/interfaces/ITimeSeriesLoader.hpp"

namespace ts {

class TimeSeriesService {
 public:
    TimeSeriesService(std::shared_ptr<CachedTimeSeriesRepository> cache, std::shared_ptr<ITimeSeriesLoader> provider);

    TimeSeries getRaw(const std::string& id, Timestamp startMs, Timestamp endMs, Timestamp coarsestMs = INT64_MAX);

    TimeSeries getAligned(const std::string& id, TimestampsPtr grid);

    TimeSeries getFilled(const std::string& id, TimestampsPtr grid,
                         InterpolationStrategy strategy = InterpolationStrategy::Nearest);
    TimeSeries getFilled(const std::string& id, Timestamp startMs, Timestamp endMs, Timestamp freqMs,
                         InterpolationStrategy strategy = InterpolationStrategy::Nearest);

    // ---- Spot ----
    double getSinglePoint(const std::string& id, Timestamp ts);
    double getSinglePointOrThrow(const std::string& id, Timestamp ts);

    // ---- transitional shims (build a regular grid, then extract) ----
    TimeSeries get(const std::string& id, Timestamp startMs, Timestamp endMs, Timestamp freqMs);
    TimeSeries getResampled(const std::string& id, Timestamp startMs, Timestamp endMs, Timestamp freqMs,
                            InterpolationStrategy strategy = InterpolationStrategy::Nearest);
    TimeSeries get(const std::string& id, TimestampsPtr grid);

 private:
    std::shared_ptr<CachedTimeSeriesRepository> cache_;
    std::shared_ptr<ITimeSeriesLoader> provider_;

    TimeSeries loadBucket_(const std::string& id, Timestamp startMs, Timestamp endMs, Timestamp coarsestMs,
                           bool finestFirst);

    std::optional<SeriesKey> selectBucket_(const std::string& id, Timestamp startMs, Timestamp endMs,
                                           Timestamp coarsestMs, bool finestFirst) const;

    double singlePoint_(const std::string& id, Timestamp ts, bool requireExact);

    void fetchAndMergeGaps_(const SeriesKey& key, const std::vector<TimeRange>& gaps);
};
}  // namespace ts
