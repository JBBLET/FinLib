// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/SeriesKey.hpp"
#include "finlib/data/TimeRange.hpp"
#include "finlib/data/implementation/CachedTimeSeriesRepository.hpp"
#include "finlib/data/interfaces/ITimeSeriesLoader.hpp"

class TimeSeriesService {
 public:
    TimeSeriesService(std::shared_ptr<CachedTimeSeriesRepository> cache, std::shared_ptr<ITimeSeriesLoader> provider);

    TimeSeries get(const std::string& id, int64_t startMs, int64_t endMs, int64_t requestedFrequencyMs);

 private:
    std::shared_ptr<CachedTimeSeriesRepository> cache_;
    std::shared_ptr<ITimeSeriesLoader> provider_;

    std::optional<SeriesKey> findLocalCoveringKey_(const std::string& id, int64_t startMs, int64_t endMs,
                                                   int64_t maxFrequencyMs) const;

    void fetchAndMergeGaps_(const SeriesKey& key, const std::vector<TimeRange>& gaps);
};
