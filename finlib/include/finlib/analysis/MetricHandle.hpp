// Copyright 2026 JBBLET
#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finlib/core/TimeSeriesView.hpp"

namespace analysis {

inline std::string joinStrings(std::vector<std::string> arr, std::string sep) {
    if (arr.empty()) return "";
    std::string out = arr[0];
    for (unsigned int i = 1; i < arr.size(); i++) out += sep + arr[i];
    return out;
}
inline std::string joinStrings(std::vector<std::string> arr) { return joinStrings(arr, std::string(", ")); }

template <typename T>
using MetricFn = std::function<T(TimeSeriesView)>;
template <typename T>
using MultiMetricFn = std::function<T(const std::unordered_map<std::string, TimeSeriesView>&)>;
template <typename T, typename P>
using ParameterizedMultiMetricFn = std::function<T(const std::unordered_map<std::string, TimeSeriesView>&, const P&)>;
template <typename T, typename P>
using ParameterizedMetricFn = std::function<T(TimeSeriesView, const P&)>;

template <typename T>
class MetricHandle {
 public:
    MetricHandle(std::vector<std::string> seriesInputs, std::string metricName)
        : seriesInputs_(std::move(seriesInputs)) {
        cacheKey_ = seriesInputs_.empty() ? metricName : metricName + ":" + joinStrings(seriesInputs_);
    }
    const std::vector<std::string>& seriesInput() const { return seriesInputs_; }
    const std::string& cacheKey() const { return cacheKey_; }

 private:
    std::vector<std::string> seriesInputs_;
    std::string cacheKey_;
};

template <typename T, typename P>
class ParameterizedMetricHandle {
 public:
    ParameterizedMetricHandle(std::vector<std::string> seriesInputs, std::string metricName)
        : seriesInputs_(std::move(seriesInputs)), metricName_(std::move(metricName)) {}

    const std::vector<std::string>& seriesName() const { return seriesInputs_; }
    const std::string& metricName() const { return metricName_; }

 private:
    std::vector<std::string> seriesInputs_;
    std::string metricName_;
};

}  // namespace analysis
