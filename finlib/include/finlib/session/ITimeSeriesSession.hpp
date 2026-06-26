// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "finlib/analysis/TimeSeriesAnalysis.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"

namespace ts::analysis {

class CustomTimeSeriesAnalysis;  // forward declaration — full type in CustomTimeSeriesAnalysis.hpp

// A named-input transform producing a derived series. The map is keyed by the
// requested input names: "" or "source" for a single session's primary series,
// sub-session / cross names for a multi-session. Top-level const on the by-value map parameter is
// dropped from the function type, so this is the same type as the concrete
// sessions' ComputeTransform / CrossTransform aliases.
using SeriesTransform =
    std::function<TimeSeries(std::unordered_map<std::string, std::shared_ptr<const TimeSeries>>)>;

// Common interface for single-series and multi-series sessions.
// Enables nesting: a MultiTimeSeriesSession can hold ITimeSeriesSession sub-nodes,
// so a MultiTimeSeriesSession can itself be a sub-node of another MultiTimeSeriesSession.
//
// Series naming convention:
//   name = ""       → primary series (raw source for TimeSeriesSession; undefined/throws for Multi)
//   name = "return" → named derived or cross-transform series
//
// This unifies sourceXxx / derivedXxx into a single access path, which simplifies
// buildAligned_ and the matrix analytics in MultiTimeSeriesSession.
class ITimeSeriesSession {
 public:
    virtual ~ITimeSeriesSession() = default;

    // Propagated to all backing data sources.
    virtual void setRange(Timestamp startMs, Timestamp endMs) = 0;
    virtual void setFrequency(Timestamp freqMs) = 0;

    // Series access by name. Empty name = primary/source series.
    // Implementations may throw std::logic_error for names they don't support.
    virtual std::shared_ptr<const TimeSeries> seriesPtr(const std::string& name) = 0;
    virtual TimeSeriesView seriesView(const std::string& name) = 0;
    virtual const TimeSeriesAnalysis& seriesAnalysis(const std::string& name) = 0;

    // Custom metric analysis for a named series. Empty name = source (TimeSeriesSession only).
    virtual CustomTimeSeriesAnalysis& customAnalysis(const std::string& seriesName = "") = 0;

    // Register a named derived series computed from the given inputs. Lets callers
    // install transforms without knowing the concrete session type (single vs multi).
    virtual void addTransform(std::string name, std::vector<std::string> inputs, SeriesTransform transform) = 0;
};

}  // namespace ts::analysis
