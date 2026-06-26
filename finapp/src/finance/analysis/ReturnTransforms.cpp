// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/finance/analysis/ReturnTransforms.hpp"

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "finlib/core/TimeSeries.hpp"

namespace finance::analysis {

ts::TimeSeries logReturns(const ts::TimeSeries& series) {
    const auto& vals = series.getValues();
    const auto ts = series.getTimestamps();
    std::vector<double> returns;
    returns.reserve(vals.size() > 0 ? vals.size() - 1 : 0);
    for (size_t i = 1; i < vals.size(); ++i) {
        if (vals[i - 1] > 0.0 && vals[i] > 0.0)
            returns.push_back(std::log(vals[i] / vals[i - 1]));
        else
            returns.push_back(0.0);
    }
    auto retTs = std::make_shared<std::vector<int64_t>>(ts.begin() + (ts.empty() ? 0 : 1), ts.end());
    return ts::TimeSeries("LogReturns_" + series.getId(), std::move(retTs), std::move(returns));
}

ts::TimeSeries simpleReturns(const ts::TimeSeries& series) {
    const auto& vals = series.getValues();
    const auto ts = series.getTimestamps();
    std::vector<double> returns;
    returns.reserve(vals.size() > 0 ? vals.size() - 1 : 0);
    for (size_t i = 1; i < vals.size(); ++i) {
        if (vals[i - 1] != 0.0)
            returns.push_back((vals[i] - vals[i - 1]) / vals[i - 1]);
        else
            returns.push_back(0.0);
    }
    auto retTs = std::make_shared<std::vector<int64_t>>(ts.begin() + (ts.empty() ? 0 : 1), ts.end());
    return ts::TimeSeries("SimpleReturns_" + series.getId(), std::move(retTs), std::move(returns));
}

}  // namespace finance::analysis
