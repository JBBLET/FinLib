// Copyright 2026 JBBLET

#pragma once

#include <any>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finlib/analysis/seriesAnalysis/ITimeSeriesAnalysis.hpp"
#include "finlib/analysis/seriesAnalysis/MetricHandle.hpp"
#include "finlib/common/logger/ILogger.hpp"
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
    logging::ILogger* logger_ = nullptr;

 public:
    // Single-series — seriesName matches the key used in addMetric
    CustomTimeSeriesAnalysis(std::string seriesName, TimeSeriesView view, logging::ILogger* logger = nullptr)
        : logger_{logger} {
        views_.emplace(std::move(seriesName), std::move(view));
    }

    // Multi-series
    explicit CustomTimeSeriesAnalysis(std::unordered_map<std::string, TimeSeriesView> views,
                                      logging::ILogger* logger = nullptr)
        : views_(std::move(views)), logger_{logger} {}

    // ITimeSeriesAnalysis — single-view rebind for single-series analyses
    void rebind(TimeSeriesView newView) override {
        if (views_.size() != 1) throw std::logic_error("rebind(view) requires exactly one registered view");
        if (logger_) logger_->write(logging::Level::Debug, "rebind");
        views_.begin()->second = std::move(newView);
        metricCache_.clear();
    }

    // Session calls this on setRange / setFrequency — keeps definitions, refreshes views
    void rebind(const std::string& seriesName, TimeSeriesView newView) {
        if (logger_) logger_->write(logging::Level::Debug, "rebind '" + seriesName + "'");
        views_.at(seriesName) = std::move(newView);
        metricCache_.clear();
    }

    void rebind(std::unordered_map<std::string, TimeSeriesView> newViews) {
        if (logger_) logger_->write(logging::Level::Debug, "rebind all views");
        views_ = std::move(newViews);
        metricCache_.clear();
    }

    // Metrics Management
    //  Single-series metric — specify which view it operates on
    template <typename T>
    MetricHandle<T> addMetric(std::string seriesName, std::string metricName, MetricFn<T> fn) {
        if (!views_.count(seriesName)) throw std::invalid_argument("No view registered for series: " + seriesName);
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
            if (!views_.count(name)) throw std::invalid_argument("No view registered for series: " + name);
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
        if (!views_.count(seriesName)) throw std::invalid_argument("No view for series: " + seriesName);
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
            if (!views_.count(name)) throw std::invalid_argument("No view registered for series: " + name);
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
        if (logger_) logger_->write(logging::Level::Debug, "compute: " + handle.cacheKey());
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
};

}  // namespace ts::analysis
