// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <concepts>
#include <format>
#include <functional>
#include <memory>
#include <print>
#include <string>
#include <unordered_map>
#include <vector>

#include "finlib/analysis/seriesAnalysis/TimeSeriesAnalysis.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/Format.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"

namespace ts::analysis {

class CustomTimeSeriesAnalysis;  // forward declaration — full type in CustomTimeSeriesAnalysis.hpp

// A named-input transform producing a derived series. The map is keyed by the
// requested input names: "" or "source" for a single session's primary series,
// sub-session / cross names for a multi-session. Top-level const on the by-value map parameter is
// dropped from the function type, so this is the same type as the concrete
// sessions' ComputeTransform / CrossTransform aliases.
using SeriesTransform = std::function<TimeSeries(std::unordered_map<std::string, std::shared_ptr<const TimeSeries>>)>;

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
    // Implementations may throw ts::Exception for names they don't support.
    virtual std::shared_ptr<const TimeSeries> seriesPtr(const std::string& name) = 0;
    virtual TimeSeriesView seriesView(const std::string& name) = 0;
    virtual const TimeSeriesAnalysis& seriesAnalysis(const std::string& name) = 0;

    // Custom metric analysis for a named series. Empty name = source (TimeSeriesSession only).
    virtual CustomTimeSeriesAnalysis& customAnalysis(const std::string& seriesName = "") = 0;

    // Register a named derived series computed from the given inputs. Lets callers
    // install transforms without knowing the concrete session type (single vs multi).
    virtual void addTransform(std::string name, std::vector<std::string> inputs, SeriesTransform transform) = 0;

    // Display. Virtual so a MultiTimeSeriesSession can summarise its sub-nodes without
    // knowing whether each is a single or another multi.
    //
    // Const, and deliberately non-building: describe reports which derived series are
    // *currently* cached rather than materialising them, so inspecting a session never
    // changes what it holds or triggers a fetch.
    virtual std::string toString(const fmt::FormatSpec& spec) const = 0;
    void println(const fmt::FormatSpec& spec = {.mode = fmt::FormatMode::Describe}) const {
        std::println("{}", toString(spec));
    }
};

}  // namespace ts::analysis

template <class T>
    requires std::derived_from<T, ts::analysis::ITimeSeriesSession>
struct std::formatter<T> {
    ts::fmt::FormatSpec spec;

    constexpr auto parse(std::format_parse_context& ctx) { return ts::fmt::parseFormatSpec(ctx, spec); }

    auto format(const T& session, std::format_context& ctx) const -> std::format_context::iterator {
        return std::format_to(ctx.out(), "{}", session.toString(spec));
    }
};
