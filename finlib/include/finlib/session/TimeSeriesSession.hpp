// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "finlib/analysis/TimeSeriesAnalysis.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"

namespace analysis {

using TimestampMs = int64_t;
using DerivedTransform = std::function<TimeSeries(const TimeSeries&)>;

class TimeSeriesSession {
 public:
    // Regular grid constructor
    TimeSeriesSession(std::shared_ptr<TimeSeriesService> service, std::string seriesId, TimestampMs startMs,
                      TimestampMs endMs, TimestampMs frequencyMs);

    // Irregular / custom timestamp grid constructor
    TimeSeriesSession(std::shared_ptr<TimeSeriesService> service, std::string seriesId,
                      std::shared_ptr<std::vector<TimestampMs>> timestampsMs);

    // Range / frequency setters
    void setRange(TimestampMs newStartMs, TimestampMs newEndMs);
    void setFrequency(TimestampMs newFrequencyMs);

    // Named derived transforms
    void addTransform(std::string name, DerivedTransform transform);

    // View accessors
    TimeSeriesView sourceView() const;
    TimeSeriesView derivedView(const std::string& name) const;

    // Analysis accessors
    const TimeSeriesAnalysis& sourceAnalysis();
    const TimeSeriesAnalysis& derivedAnalysis(const std::string& name);

    // Scalar accessors
    TimestampMs startMs() const { return startMs_; }
    TimestampMs endMs() const { return endMs_; }
    const std::string& seriesId() const { return seriesId_; }
    size_t size() const;
    TimestampMs frequencyMs() const;

 private:
    std::shared_ptr<TimeSeriesService> service_;
    std::shared_ptr<const TimeSeries> source_;

    std::unordered_map<std::string, DerivedTransform> transforms_;
    mutable std::unordered_map<std::string, std::shared_ptr<const TimeSeries>> derivedCaches_;
    mutable std::unordered_map<std::string, std::optional<TimeSeriesAnalysis>> derivedAnalysisCache_;

    std::string seriesId_;
    TimestampMs startMs_;
    TimestampMs endMs_;
    std::optional<TimestampMs> frequencyMs_;

    std::optional<TimeSeriesAnalysis> sourceAnalysis_;

    void buildDerived_(const std::string& name) const;
    void invalidateAllCache_();
    void extendRange_(TimestampMs newStartMs, TimestampMs newEndMs);
};

}  // namespace analysis
