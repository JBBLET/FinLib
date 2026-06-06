// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "finlib/analysis/TimeSeriesAnalysis.hpp"
#include "finlib/common/logger/ILogger.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"

namespace analysis {

using TimestampMs = int64_t;
using DerivedTransform = std::function<TimeSeries(const TimeSeries&)>;

class TimeSeriesSession {
 public:
    // Constructors
    TimeSeriesSession(std::shared_ptr<TimeSeriesService> service, std::string seriesId, int64_t startMs,
                      TimestampMs endMs, TimestampMs frequencyMs, logging::ILogger* logger = nullptr);

    TimeSeriesSession(std::shared_ptr<TimeSeriesService> service, std::string seriesId,
                      std::shared_ptr<std::vector<TimestampMs>> timestampsMs, logging::ILogger* logger = nullptr);

    // Setters Methods
    void setRange(TimestampMs newStartMs, TimestampMs newEndMs);
    void setFrequency(TimestampMs newFrequencyMs);
    void setTransform(DerivedTransform transform);

    // View Accessors
    TimeSeriesView sourceView() const;
    TimeSeriesView derivedView() const;

    // analysis Accessors
    const TimeSeriesAnalysis& sourceAnalysis();
    const TimeSeriesAnalysis& derivedAnalysis();

    // Accessors
    TimestampMs startMs() const { return startMs_; }
    TimestampMs endMs() const { return endMs_; }
    const std::string& seriesId() const { return seriesId_; }
    size_t size() const;
    TimestampMs frequencyMs() const;

 private:
    std::shared_ptr<TimeSeriesService> service_;
    std::shared_ptr<const TimeSeries> source_;
    std::optional<DerivedTransform> transform_;
    mutable std::shared_ptr<const TimeSeries> derivedCache_;
    std::unique_ptr<logging::ILogger> logger_;

    std::string seriesId_;
    TimestampMs startMs_;
    TimestampMs endMs_;
    std::optional<TimestampMs> frequencyMs_;

    std::optional<TimeSeriesAnalysis> sourceAnalysis_;
    std::optional<TimeSeriesAnalysis> derivedAnalysis_;

    // helpers
    void buildDerived_() const;
    void invalidateAllCache_();
    void extendRange_(TimestampMs newStartMs, TimestampMs newEndMs);
    void reduceRange_(TimestampMs newStartMs, TimestampMs newEndMs);
};
}  // namespace analysis
