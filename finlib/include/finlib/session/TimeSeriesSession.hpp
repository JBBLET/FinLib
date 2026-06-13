// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "finlib/analysis/TimeSeriesAnalysis.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"

namespace analysis {

using DerivedTransform = std::function<TimeSeries(const TimeSeries&)>;

class TimeSeriesSession {
 public:
    // Regular grid constructor
    TimeSeriesSession(std::shared_ptr<TimeSeriesService> service, std::string seriesId, Timestamp startMs,
                      Timestamp endMs, Timestamp frequencyMs);

    // Irregular / custom timestamp grid constructor
    TimeSeriesSession(std::shared_ptr<TimeSeriesService> service, std::string seriesId,
                      std::shared_ptr<Timestamps> timestampsMs);

    // Computed series — source is pre-built, no backing service (range cannot be extended)
    explicit TimeSeriesSession(std::shared_ptr<const TimeSeries> precomputed);

    // Range / frequency setters
    void setRange(Timestamp newStartMs, Timestamp newEndMs);
    void setFrequency(Timestamp newFrequencyMs);

    // Named derived transforms
    void addTransform(std::string name, DerivedTransform transform);

    // View accessors
    TimeSeriesView sourceView() const;
    TimeSeriesView derivedView(const std::string& name) const;

    // Analysis accessors
    const TimeSeriesAnalysis& sourceAnalysis();
    const TimeSeriesAnalysis& derivedAnalysis(const std::string& name);

    // Scalar accessors
    Timestamp startMs() const { return startMs_; }
    Timestamp endMs() const { return endMs_; }
    const std::string& seriesId() const { return seriesId_; }
    size_t size() const;
    Timestamp frequencyMs() const;

 private:
    std::shared_ptr<TimeSeriesService> service_;
    std::shared_ptr<const TimeSeries> source_;

    std::unordered_map<std::string, DerivedTransform> transforms_;
    mutable std::unordered_map<std::string, std::shared_ptr<const TimeSeries>> derivedCaches_;
    mutable std::unordered_map<std::string, std::optional<TimeSeriesAnalysis>> derivedAnalysisCache_;

    std::string seriesId_;
    Timestamp startMs_;
    Timestamp endMs_;
    std::optional<Timestamp> frequencyMs_;

    std::optional<TimeSeriesAnalysis> sourceAnalysis_;

    void buildDerived_(const std::string& name) const;
    void invalidateAllCache_();
    void extendRange_(Timestamp newStartMs, Timestamp newEndMs);
};

}  // namespace analysis
