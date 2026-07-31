// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "finlib/analysis/seriesAnalysis/CustomTimeSeriesAnalysis.hpp"
#include "finlib/analysis/seriesAnalysis/TimeSeriesAnalysis.hpp"
#include "finlib/analysis/session/ITimeSeriesSession.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"

namespace ts::analysis {

using DerivedTransform = std::function<TimeSeries(const TimeSeries&)>;
using ComputeTransform = std::function<TimeSeries(std::unordered_map<std::string, std::shared_ptr<const TimeSeries>>)>;

class TimeSeriesSession : public ITimeSeriesSession {
    struct SeriesNode {
        std::string name;
        std::vector<std::string> inputs;
        ComputeTransform transform;
    };

 public:
    // Regular grid constructor
    TimeSeriesSession(std::shared_ptr<TimeSeriesService> service, std::string seriesId, Timestamp startMs,
                      Timestamp endMs, Timestamp frequencyMs);

    // Irregular / custom timestamp grid constructor
    TimeSeriesSession(std::shared_ptr<TimeSeriesService> service, std::string seriesId, TimestampsPtr timestampsMs);

    // Computed series — source is pre-built, no backing service (range cannot be extended)
    explicit TimeSeriesSession(std::shared_ptr<const TimeSeries> precomputed);

    // Range / frequency setters
    void setRange(Timestamp newStartMs, Timestamp newEndMs) override;
    void setFrequency(Timestamp newFrequencyMs) override;

    // Named derived transforms
    void addTransform(std::string name, DerivedTransform transform);
    void addTransform(std::string name, std::vector<std::string> inputs, ComputeTransform transform) override;

    // ITimeSeriesSession — name = "" → source, non-empty → named derived transform
    std::shared_ptr<const TimeSeries> seriesPtr(const std::string& name) override;
    TimeSeriesView seriesView(const std::string& name) override;
    const TimeSeriesAnalysis& seriesAnalysis(const std::string& name) override;

    // Concrete accessors kept for direct use
    std::shared_ptr<const TimeSeries> sourceTimeSeriesPtr() { return source_; }
    std::shared_ptr<const TimeSeries> derivedTimeSeriesPtr(const std::string& name);

    TimeSeriesView sourceView() const;
    TimeSeriesView derivedView(const std::string& name) const;

    const TimeSeriesAnalysis& sourceAnalysis();
    const TimeSeriesAnalysis& derivedAnalysis(const std::string& name);

    // Custom metric analysis — created lazily, rebound on range/frequency change
    CustomTimeSeriesAnalysis& customAnalysis(const std::string& seriesName = "") override;

    // Scalar accessors
    Timestamp startMs() const { return startMs_; }
    Timestamp endMs() const { return endMs_; }
    const std::string& seriesId() const { return seriesId_; }
    size_t size() const;
    Timestamp frequencyMs() const;

 private:
    std::shared_ptr<TimeSeriesService> service_;
    std::shared_ptr<const TimeSeries> source_;

    std::unordered_map<std::string, SeriesNode> transforms_;
    mutable std::unordered_map<std::string, std::shared_ptr<const TimeSeries>> derivedCaches_;
    mutable std::unordered_map<std::string, std::optional<TimeSeriesAnalysis>> derivedAnalysisCache_;

    std::string seriesId_;
    Timestamp startMs_;
    Timestamp endMs_;
    std::optional<Timestamp> frequencyMs_;

    std::optional<TimeSeriesAnalysis> sourceAnalysis_;

    std::optional<CustomTimeSeriesAnalysis> sourceCustomAnalysis_;
    std::unordered_map<std::string, std::optional<CustomTimeSeriesAnalysis>> derivedCustomAnalysisCache_;


    void buildDerived_(const std::string& name) const;
    void invalidateAllCache_();
    void extendRange_(Timestamp newStartMs, Timestamp newEndMs);
};

}  // namespace ts::analysis
