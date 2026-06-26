// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/finance/analysis/EquityAnalysis.hpp"

#include <memory>
#include <utility>

#include "finapp/service/AssetService.hpp"

namespace finance::analysis {

EquityAnalysis::EquityAnalysis(std::shared_ptr<const finance::Equity> equity,
                               std::shared_ptr<TimeSeriesSession> session,
                               std::shared_ptr<finapp::AssetService> assetService)
    : IAssetAnalysis{std::move(equity), std::move(session)}, assetService_{std::move(assetService)} {}

}  // namespace finance::analysis
