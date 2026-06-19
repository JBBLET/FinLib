// Copyright 2026 JBBLET
#pragma once

#include "finlib/analysis/MetricHandle.hpp"
#include "finlib/core/TimeSeriesView.hpp"

namespace analysis {

// Common interface for single-series and custom analysis objects.
//
// addMetric / compute / removeMetric are template methods and cannot be virtual —
// they are available on all concrete types (CustomTimeSeriesAnalysis, TimeSeriesAnalysis)
// with identical signatures.
class ITimeSeriesAnalysis {
 public:
    virtual ~ITimeSeriesAnalysis() = default;

    // Clears all cached metric results. Definitions survive.
    virtual void invalidateCache() = 0;

    // Replaces the underlying view and clears the cache.
    // For single-series analyses only — multi-series use the map overload on the concrete type.
    virtual void rebind(TimeSeriesView newView) = 0;
};

}  // namespace analysis
