// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "finlib/analysis/TimeSeriesAnalysis.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/session/TimeSeriesSession.hpp"

namespace analysis {

// TODO(JBBLET) Add AlignmentPolicy (Intersection / Union) support for sessions that do not share a
//       timestamp grid. Currently assumes all sessions were constructed with the same shared
//       TimestampPtr or the same (startMs, endMs, frequencyMs) so their views are already aligned.

using CrossTransform = std::function<TimeSeries(const std::unordered_map<std::string, TimeSeries>&)>;

// Holds N named TimeSeriesSessions and supports cross-series operations
// (NAV aggregation, correlation/covariance matrices, arbitrary cross-transforms).
// setRange / setFrequency propagate to every sub-session.
class MultiTimeSeriesSession {
 public:
    MultiTimeSeriesSession() = default;

    void addSession(std::string name, std::shared_ptr<TimeSeriesSession> session);

    // Propagates to all sub-sessions and invalidates cross-series caches.
    void setRange(Timestamp startMs, Timestamp endMs);
    void setFrequency(Timestamp freqMs);

    // Per-series — delegates to the named sub-session
    TimeSeriesView seriesView(const std::string& name) const;
    const TimeSeriesAnalysis& seriesAnalysis(const std::string& name);
    TimeSeriesView derivedSeriesView(const std::string& name, const std::string& transform) const;
    const TimeSeriesAnalysis& derivedSeriesAnalysis(const std::string& name, const std::string& transform);

    // Cross-series transforms — called with materialized source TimeSeries for every session.
    // Results are cached and invalidated whenever setRange / setFrequency is called.
    void addCrossTransform(std::string name, CrossTransform fn);
    TimeSeriesView crossView(const std::string& name);
    const TimeSeriesAnalysis& crossAnalysis(const std::string& name);

    // Matrix analytics computed over the named sessions.
    // If transformName is empty the source series is used; otherwise the named derived series.
    std::vector<std::vector<double>> correlationMatrix(const std::vector<std::string>& names,
                                                       const std::string& transformName = "");
    std::vector<std::vector<double>> covarianceMatrix(const std::vector<std::string>& names,
                                                      const std::string& transformName = "");

    std::vector<std::string> sessionNames() const;

 private:
    std::unordered_map<std::string, std::shared_ptr<TimeSeriesSession>> sessions_;
    std::unordered_map<std::string, CrossTransform> crossTransforms_;
    mutable std::unordered_map<std::string, std::shared_ptr<const TimeSeries>> crossCaches_;
    mutable std::unordered_map<std::string, std::optional<TimeSeriesAnalysis>> crossAnalysisCache_;

    std::unordered_map<std::string, TimeSeries> buildAligned_() const;
    void invalidateCross_();
};

}  // namespace analysis
