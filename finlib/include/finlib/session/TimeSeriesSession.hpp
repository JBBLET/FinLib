// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "finlib/analysis/TimeSeriesAnalysis.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"

using TimestampMs = int64_t;
using DerivedTransform = std::function<TimeSeries(const TimeSeries&)>;

namespace analysis {
class TimeSeriesSession {
 public:
    TimeSeriesSession();

    TimeSeriesSession& extendRange(TimestampMs newStartMs, TimestampMs newEndMs);
    TimeSeriesSession& setFrequency(TimestampMs newFrequencyMs);
    void ReBuildTimeSeriesSession();

    TimeSeriesAnalysis& buildSourceAnalysis();
    TimeSeriesAnalysis& buildDerivedAnalysis();

    TimeSeriesView& sourceView() const;
    TimeSeriesView& derivedView() const;

    TimestampMs startMs() const { return startMs_; }
    TimestampMs endMs() const { return endMs_; }
    TimestampMs frequencyMs() const { return frequencyMs_; }
    const std::string& seriesId() const { return seriesId_; }
    size_t size() const;

 private:
    TimeSeries source_;
    std::optional<DerivedTransform> transform_;
    std::optional<TimeSeries> derivedCache_;  // TODO(JBBLET) Currently second full copy of the TimeSeries due to how
                                              // apply rebuild a TimeSeries by Value each time)
    std::string seriesId_;
    TimestampMs startMs_;
    TimestampMs endMs_;
    TimestampMs frequencyMs_;

    std::optional<TimeSeriesAnalysis> sourceAnalysis_;
    std::optional<TimeSeriesAnalysis> derivedAnalysis_;
    void buildDerived_();
    void invalidateAllCache_();
};
}  // namespace analysis
