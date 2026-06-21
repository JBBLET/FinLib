// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "finlib/analysis/CustomTimeSeriesAnalysis.hpp"
#include "finlib/analysis/TimeSeriesAnalysis.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/logger/ILogger.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/session/ITimeSeriesSession.hpp"

namespace analysis {

// TODO(JBBLET) Add AlignmentPolicy (Intersection / Union) support for sessions that do not share a
//       timestamp grid. Currently assumes all sessions were constructed with the same shared
//       TimestampPtr or the same (startMs, endMs, frequencyMs) so their views are already aligned.

using CrossTransform =
    std::function<TimeSeries(const std::unordered_map<std::string, std::shared_ptr<const TimeSeries>>)>;

// Holds N named TimeSeriesSessions and supports cross-series operations
// (NAV aggregation, correlation/covariance matrices, arbitrary cross-transforms).
// setRange / setFrequency propagate to every sub-session.
class MultiTimeSeriesSession : public ITimeSeriesSession {
    struct SeriesNode {
        // Used to represent a derived Series and how to reach it to be able to create derivation  of derivation Similar
        // to SeriesLeaf in TimeSeriesSession this way can build a graph of dependency to build back
        std::string name;
        std::vector<std::string> inputs;
        CrossTransform crossTransform;
    };

 public:
    explicit MultiTimeSeriesSession(logging::ILogger* logger = nullptr);

    // ITimeSeriesSession — name = "" throws (no single source); non-empty = cross-transform result
    std::shared_ptr<const TimeSeries> seriesPtr(const std::string& name) override;
    TimeSeriesView seriesView(const std::string& name) override;
    const TimeSeriesAnalysis& seriesAnalysis(const std::string& name) override;

    void setRange(Timestamp startMs, Timestamp endMs) override;
    void setFrequency(Timestamp freqMs) override;

    // Sub-session registration — accepts any ITimeSeriesSession (TimeSeriesSession or another Multi)
    void addSession(std::string name, std::shared_ptr<ITimeSeriesSession> session);
    void addSession(std::unordered_map<std::string, std::shared_ptr<ITimeSeriesSession>> sessionMap);

    // Per-session access — delegates to the named sub-session
    TimeSeriesView subSeriesView(const std::string& sessionName, const std::string& seriesName = "");
    const TimeSeriesAnalysis& subSeriesAnalysis(const std::string& sessionName, const std::string& seriesName = "");

    // Custom metric analysis for a named cross-transform series
    CustomTimeSeriesAnalysis& customAnalysis(const std::string& seriesName) override;
    // Reach into a sub-session's custom analysis
    CustomTimeSeriesAnalysis& subCustomAnalysis(const std::string& sessionName, const std::string& seriesName = "");

    // Cross-series transforms — results are cached and invalidated on setRange / setFrequency.
    // No inputs = implicitly depends on all current sessions (snapshot at registration time).
    void addTransform(std::string name, CrossTransform fn);
    void addTransform(std::string name, std::vector<std::string> inputs, CrossTransform fn);
    void addTransform(SeriesNode node);

    std::vector<std::string> sessionNames() const;

 private:
    std::unordered_map<std::string, std::shared_ptr<ITimeSeriesSession>> sessions_;
    std::list<std::string> sessionNames_;
    std::unordered_map<std::string, std::vector<std::string>> reverseDeps_;
    std::unordered_map<std::string, SeriesNode> crossTransforms_;
    mutable std::unordered_map<std::string, std::shared_ptr<const TimeSeries>> crossCaches_;
    mutable std::unordered_map<std::string, std::optional<TimeSeriesAnalysis>> crossAnalysisCache_;

    std::unordered_map<std::string, std::optional<CustomTimeSeriesAnalysis>> crossCustomAnalysisCache_;

    std::unique_ptr<logging::ILogger> logger_;

    std::unordered_map<std::string, std::shared_ptr<const TimeSeries>> buildAligned_(const std::string& name) const;
    void buildCross_(const std::string& name) const;
    void invalidateAll_();
    void invalidate_(const std::string& name);
};

}  // namespace analysis
