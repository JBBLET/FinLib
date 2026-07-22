// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <any>
#include <cmath>
#include <functional>
#include <string>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

#include "finlib/analysis/CustomTimeSeriesAnalysis.hpp"
#include "finlib/analysis/MetricHandle.hpp"
#include "finlib/session/ITimeSeriesSession.hpp"

namespace finance::analysis {

struct RetainedMetric {
    std::string seriesName;   //
    std::string displayName;  //
    std::type_index resultType;
    std::function<std::any(ts::analysis::ITimeSeriesSession&)> evaluate;
};

struct FeatureBindings {
    std::vector<std::string> derivedSeries;
    std::vector<RetainedMetric> metrics;
};

using FeatureInstaller = std::function<FeatureBindings(ts::analysis::ITimeSeriesSession&, const std::string& base)>;

template <typename T>
RetainedMetric retainMetric(std::string seriesName, std::string displayName, ts::analysis::MetricHandle<T> handle) {
    auto evaluate = [seriesName, handle](ts::analysis::ITimeSeriesSession& session) -> std::any {
        return std::any(session.customAnalysis(seriesName).compute(handle));
    };
    return RetainedMetric{
        std::move(seriesName), std::move(displayName), std::type_index(typeid(T)), std::move(evaluate)};
}

template <typename T, typename P>
RetainedMetric retainMetric(std::string seriesName, std::string displayName,
                            ts::analysis::ParameterizedMetricHandle<T, P> handle, P param) {
    auto evaluate = [seriesName, handle, param](ts::analysis::ITimeSeriesSession& session) -> std::any {
        return std::any(session.customAnalysis(seriesName).compute(handle, param));
    };
    return RetainedMetric{
        std::move(seriesName), std::move(displayName), std::type_index(typeid(T)), std::move(evaluate)};
}

template <typename T>
std::vector<std::pair<std::string, T>> computeMetricsOfType(ts::analysis::ITimeSeriesSession& session,
                                                            const std::vector<RetainedMetric>& metrics) {
    std::vector<std::pair<std::string, T>> out;
    const std::type_index want(typeid(T));
    for (const auto& m : metrics) {
        if (m.resultType != want) continue;
        try {
            T value = std::any_cast<T>(m.evaluate(session));
            if constexpr (std::is_floating_point_v<T>) {
                if (!std::isfinite(value)) continue;
            }
            out.emplace_back(m.displayName, std::move(value));
        } catch (const std::exception&) {
        }
    }
    return out;
}

}  // namespace finance::analysis
