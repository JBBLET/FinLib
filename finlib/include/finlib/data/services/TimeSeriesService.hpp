// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "finlib/common/logger/ILogger.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/SeriesKey.hpp"
#include "finlib/data/TimeRange.hpp"
#include "finlib/data/implementation/CachedTimeSeriesRepository.hpp"
#include "finlib/data/interfaces/ITimeSeriesLoader.hpp"

class TimeSeriesService {
 public:
    TimeSeriesService(std::shared_ptr<CachedTimeSeriesRepository> cache, std::shared_ptr<ITimeSeriesLoader> provider,
                      logging::ILogger* logger = nullptr);

    // Returns raw cache/provider data at native trading-day timestamps.
    // Gaps (weekends, holidays) are NOT filled — use getResampled for that.
    TimeSeries get(const std::string& id, int64_t startMs, int64_t endMs, int64_t requestedFrequencyMs);

    // Returns raw data without resampling — supports non-regular time series
    TimeSeries getRaw(const std::string& id, int64_t startMs, int64_t endMs);

    // Overload taking a caller-owned, regularly-spaced timestamp grid. The returned
    // TimeSeries shares the same TimestampPtr, so multiple series fetched on the same
    // grid stay pointer-aligned for downstream operators.
    TimeSeries get(const std::string& id, TimestampPtr timestamps);

    // Like get(), but always returns a regularly-spaced series covering [startMs, endMs]
    // at requestedFrequencyMs intervals.  Missing points (weekends, holidays, tail data not
    // yet published) are filled via the chosen InterpolationStrategy (default: Nearest).
    // Nearest is correct for spot-price / weight lookups; Linear or Stochastic can be used
    // for chart or simulation paths.
    TimeSeries getResampled(const std::string& id, int64_t startMs, int64_t endMs, int64_t requestedFrequencyMs,
                            InterpolationStrategy strategy = InterpolationStrategy::Nearest);

 private:
    std::shared_ptr<CachedTimeSeriesRepository> cache_;
    std::shared_ptr<ITimeSeriesLoader> provider_;
    std::unique_ptr<logging::ILogger> logger_;

    std::optional<SeriesKey> findLocalCoveringKey_(const std::string& id, int64_t startMs, int64_t endMs,
                                                   int64_t maxFrequencyMs) const;

    void fetchAndMergeGaps_(const SeriesKey& key, const std::vector<TimeRange>& gaps);
};
