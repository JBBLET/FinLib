// Copyright 2026 JBBLET

#pragma once

#include <algorithm>
#include <any>
#include <format>
#include <print>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finlib/analysis/seriesAnalysis/ITimeSeriesAnalysis.hpp"
#include "finlib/analysis/seriesAnalysis/MetricHandle.hpp"
#include "finlib/common/Error.hpp"
#include "finlib/common/Format.hpp"
#include "finlib/common/Log.hpp"
#include "finlib/core/TimeSeriesView.hpp"

namespace ts::analysis {

class CustomTimeSeriesAnalysis : public ITimeSeriesAnalysis {
    struct MetricEntry {
        std::vector<std::string> inputs;
        std::function<std::any(const std::unordered_map<std::string, TimeSeriesView>&)> fn;
    };

    struct ParameterizedMetricEntry {
        std::vector<std::string> inputs;
        std::function<std::any(const std::unordered_map<std::string, TimeSeriesView>&, const std::any&)> fn;
    };

    std::unordered_map<std::string, ParameterizedMetricEntry> paramMetricFns_;
    std::unordered_map<std::string, TimeSeriesView> views_;   // value — cheap, rebindable
    std::unordered_map<std::string, MetricEntry> metricFns_;  // permanent — survive rebind
    std::unordered_map<std::string, std::any> metricCache_;

 public:
    // Single-series — seriesName matches the key used in addMetric
    CustomTimeSeriesAnalysis(std::string seriesName, TimeSeriesView view) {
        views_.emplace(std::move(seriesName), std::move(view));
    }

    // Multi-series
    explicit CustomTimeSeriesAnalysis(std::unordered_map<std::string, TimeSeriesView> views)
        : views_(std::move(views)) {}

    // ITimeSeriesAnalysis — single-view rebind for single-series analyses
    void rebind(TimeSeriesView newView) override {
        ensure(views_.size() == 1, "rebind(view) requires exactly one registered view, have {}", views_.size());
        logging::debug("rebind");
        views_.begin()->second = std::move(newView);
        metricCache_.clear();
    }

    // Session calls this on setRange / setFrequency — keeps definitions, refreshes views
    void rebind(const std::string& seriesName, TimeSeriesView newView) {
        logging::debug("rebind '{}'", seriesName);
        views_.at(seriesName) = std::move(newView);
        metricCache_.clear();
    }

    void rebind(std::unordered_map<std::string, TimeSeriesView> newViews) {
        logging::debug("rebind all views");
        views_ = std::move(newViews);
        metricCache_.clear();
    }

    // Metrics Management
    //  Single-series metric — specify which view it operates on
    template <typename T>
    MetricHandle<T> addMetric(std::string seriesName, std::string metricName, MetricFn<T> fn) {
        ensure<InvalidArgument>(views_.count(seriesName) != 0, "No view registered for series: {}", seriesName);
        MetricHandle<T> handle({seriesName}, metricName);
        std::string sn = seriesName;
        metricFns_[handle.cacheKey()] = MetricEntry{
            {std::move(seriesName)},
            [fn = std::move(fn), sn](const std::unordered_map<std::string, TimeSeriesView>& views) -> std::any {
                return fn(views.at(sn));
            }};
        metricCache_.erase(handle.cacheKey());
        return handle;
    }

    // Multi-series metric — fn receives only the requested inputs subset
    template <typename T>
    MetricHandle<T> addMetric(std::vector<std::string> inputs, std::string metricName, MultiMetricFn<T> fn) {
        for (const auto& name : inputs)
            ensure<InvalidArgument>(views_.count(name) != 0, "No view registered for series: {}", name);
        MetricHandle<T> handle(inputs, metricName);
        metricFns_[handle.cacheKey()] = MetricEntry{
            inputs, [fn = std::move(fn)](const std::unordered_map<std::string, TimeSeriesView>& views) -> std::any {
                return fn(views);
            }};
        metricCache_.erase(handle.cacheKey());
        return handle;
    }

    template <typename T, typename P>
    ParameterizedMetricHandle<T, P> addMetric(std::string seriesName, std::string metricName,
                                              ParameterizedMetricFn<T, P> fn) {
        ensure<InvalidArgument>(views_.count(seriesName) != 0, "No view for series: {}", seriesName);
        ParameterizedMetricHandle<T, P> handle({seriesName}, metricName);
        std::string sn = seriesName;
        paramMetricFns_[metricName] = ParameterizedMetricEntry{
            {std::move(seriesName)},
            [fn = std::move(fn), sn](const std::unordered_map<std::string, TimeSeriesView>& views,
                                     const std::any& p) -> std::any { return fn(views.at(sn), std::any_cast<P>(p)); }};
        return handle;
    }

    template <typename T, typename P>
    ParameterizedMetricHandle<T, P> addMetric(std::vector<std::string> inputs, std::string metricName,
                                              ParameterizedMultiMetricFn<T, P> fn) {
        for (const auto& name : inputs)
            ensure<InvalidArgument>(views_.count(name) != 0, "No view registered for series: {}", name);
        ParameterizedMetricHandle<T, P> handle(inputs, metricName);
        paramMetricFns_[metricName] = ParameterizedMetricEntry{
            inputs, [fn](const std::unordered_map<std::string, TimeSeriesView>& views, const std::any& p) -> std::any {
                return fn(views, std::any_cast<P>(p));
            }};
        return handle;
    }

    // Cached metric — erase definition and any cached result
    template <typename T>
    void removeMetric(const MetricHandle<T>& handle) {
        metricFns_.erase(handle.cacheKey());
        metricCache_.erase(handle.cacheKey());
    }

    // Parameterized metric — nothing cached, just erase the definition
    template <typename T, typename P>
    void removeMetric(const ParameterizedMetricHandle<T, P>& handle) {
        paramMetricFns_.erase(handle.metricName());
    }

    // Compute
    template <typename T>
    T compute(const MetricHandle<T>& handle) {
        if (auto it = metricCache_.find(handle.cacheKey()); it != metricCache_.end())
            return std::any_cast<T>(it->second);
        logging::debug("compute: {}", handle.cacheKey());
        auto& entry = metricFns_.at(handle.cacheKey());
        std::unordered_map<std::string, TimeSeriesView> inputViews;
        for (const auto& name : entry.inputs) inputViews.emplace(name, views_.at(name));
        auto result = entry.fn(inputViews);
        metricCache_.emplace(handle.cacheKey(), result);
        return std::any_cast<T>(result);
    }

    template <typename T, typename P>
    T compute(const ParameterizedMetricHandle<T, P>& handle, const P& param) {
        auto& entry = paramMetricFns_.at(handle.metricName());
        std::unordered_map<std::string, TimeSeriesView> inputViews;
        for (const auto& name : entry.inputs) inputViews.emplace(name, views_.at(name));
        return std::any_cast<T>(entry.fn(inputViews, std::any(param)));
    }

    void invalidateCache() override { metricCache_.clear(); }

    // Display — which views are bound, which metrics are registered, and which of those
    // currently hold a cached result. metricCache_ is keyed by an assembled cache key and
    // holds std::any, so there is otherwise no way to see what a rebind just discarded.
    std::string toString(const fmt::FormatSpec& spec) const {
        const std::string identity = std::format("CustomTimeSeriesAnalysis [{} view(s), {} metric(s), {} cached]",
                                                 views_.size(),
                                                 metricFns_.size() + paramMetricFns_.size(),
                                                 metricCache_.size());
        if (spec.mode == fmt::FormatMode::Identity) return identity;

        std::string out = identity;
        out += '\n';

        if (!views_.empty()) {
            std::vector<std::string> names;
            names.reserve(views_.size());
            for (const auto& [name, view] : views_) names.push_back(name);
            std::sort(names.begin(), names.end());

            fmt::Table table({"view", "bound to"}, {fmt::Table::Align::Left, fmt::Table::Align::Left});
            for (const auto& name : names) {
                // The empty key is the single-series convention from TimeSeriesSession.
                table.addRow({name.empty() ? "(source)" : name, views_.at(name).toString({})});
            }
            out += table.render();
        }

        std::vector<std::pair<std::string, bool>> metrics;
        metrics.reserve(metricFns_.size() + paramMetricFns_.size());
        for (const auto& [key, entry] : metricFns_) metrics.emplace_back(key, metricCache_.contains(key));
        // Parameterized metrics are never cached — the result depends on an argument that is
        // not part of the key — so they are reported as such rather than as a cold cache.
        for (const auto& [name, entry] : paramMetricFns_) metrics.emplace_back(name + " (parameterized)", false);
        std::sort(metrics.begin(), metrics.end());

        if (!metrics.empty()) {
            fmt::Table table({"metric", "cached"}, {fmt::Table::Align::Left, fmt::Table::Align::Left});
            for (const auto& [key, cached] : metrics) table.addRow({key, cached ? "yes" : "no"});
            out += '\n';
            out += table.render();
        }
        return out;
    }

    void println(const fmt::FormatSpec& spec = {.mode = fmt::FormatMode::Describe}) const {
        std::println("{}", toString(spec));
    }
};

}  // namespace ts::analysis

template <>
struct std::formatter<ts::analysis::CustomTimeSeriesAnalysis> {
    ts::fmt::FormatSpec spec;

    constexpr auto parse(std::format_parse_context& ctx) { return ts::fmt::parseFormatSpec(ctx, spec); }

    auto format(const ts::analysis::CustomTimeSeriesAnalysis& analysis, std::format_context& ctx) const
        -> std::format_context::iterator {
        return std::format_to(ctx.out(), "{}", analysis.toString(spec));
    }
};
