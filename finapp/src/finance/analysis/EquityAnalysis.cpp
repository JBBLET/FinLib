// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/finance/analysis/EquityAnalysis.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "finapp/service/AssetService.hpp"
#include "finlib/core/TimeSeries.hpp"

namespace finance::analysis {

static TimeSeries logReturnTransform(const TimeSeries& prices) {
    const auto& vals = prices.getValues();
    const auto ts = prices.getTimestamps();
    std::vector<double> returns;
    returns.reserve(vals.size() > 0 ? vals.size() - 1 : 0);
    for (size_t i = 1; i < vals.size(); ++i) returns.push_back(std::log(vals[i] / vals[i - 1]));
    auto retTs = std::make_shared<std::vector<int64_t>>(ts.begin() + 1, ts.end());
    return TimeSeries("LogReturns_" + prices.getId(), std::move(retTs), std::move(returns));
}

static TimeSeries simpleReturnTransform(const TimeSeries& prices) {
    const auto& vals = prices.getValues();
    const auto ts = prices.getTimestamps();
    std::vector<double> returns;
    returns.reserve(vals.size() > 0 ? vals.size() - 1 : 0);
    for (size_t i = 1; i < vals.size(); ++i) returns.push_back((vals[i] - vals[i - 1]) / vals[i - 1]);
    auto retTs = std::make_shared<std::vector<int64_t>>(ts.begin() + 1, ts.end());
    return TimeSeries("SimpleReturns_" + prices.getId(), std::move(retTs), std::move(returns));
}

EquityAnalysis::EquityAnalysis(std::shared_ptr<finance::Equity> equity,
                               std::shared_ptr<::analysis::TimeSeriesSession> session,
                               std::shared_ptr<finapp::AssetService> assetService)
    : IAssetAnalysis{std::move(equity), std::move(session)}, assetService_{std::move(assetService)} {
    session_->addTransform("logReturn", logReturnTransform);
    session_->addTransform("simpleReturn", simpleReturnTransform);
}

}  // namespace finance::analysis
