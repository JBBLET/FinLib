// Copyright 2026 JBBLET
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "finapp/finance/stats/FinanceStats.hpp"
#include "finlib/analysis/seriesAnalysis/MetricHandle.hpp"
#include "finlib/core/TimeSeriesView.hpp"

using ts::TimeSeriesView;
namespace finapp::metrics {

inline ts::analysis::MetricFn<double> totalReturn() {
    return [](TimeSeriesView v) { return finapp::stats::totalReturn(v); };
}

inline ts::analysis::MetricFn<double> annualizedSharpe(double annualizationFactor = 252.0) {
    return [annualizationFactor](TimeSeriesView v) { return finapp::stats::sharpeRatio(v, annualizationFactor); };
}

inline ts::analysis::MetricFn<double> annualizedVolatility(double annualizationFactor = 252.0) {
    return [annualizationFactor](TimeSeriesView v) { return finapp::stats::volatility(v, annualizationFactor); };
}

inline ts::analysis::MultiMetricFn<std::vector<std::vector<double>>> correlationMatrix() {
    return [](const std::unordered_map<std::string, TimeSeriesView>& views) {
        std::vector<const TimeSeriesView*> ordered;
        ordered.reserve(views.size());
        for (const auto& [_, v] : views) ordered.push_back(&v);
        return finapp::stats::correlationMatrix(ordered);
    };
}

}  // namespace finapp::metrics
