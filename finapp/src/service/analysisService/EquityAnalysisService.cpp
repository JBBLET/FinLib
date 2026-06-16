// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/service/analysisService/AssetsAnalysis/EquityAnalysisService.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

#include "finapp/finance/analysis/EquityAnalysis.hpp"
#include "finapp/finance/analysis/IAssetAnalysis.hpp"
#include "finapp/finance/asset/Equity.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finlib/session/TimeSeriesSession.hpp"

namespace finapp {

EquityAnalysisService::EquityAnalysisService(std::shared_ptr<AssetService> assetService)
    : assetService_{std::move(assetService)} {}

std::shared_ptr<finance::analysis::IAssetAnalysis> EquityAnalysisService::createAnalysisFromSession(
    std::shared_ptr<const finance::IAsset> asset, std::shared_ptr<::analysis::TimeSeriesSession> session) {
    if (auto equityPtr = std::dynamic_pointer_cast<const finance::Equity>(asset)) {
        return std::make_shared<finance::analysis::EquityAnalysis>(
            finance::analysis::EquityAnalysis(equityPtr, session, assetService_));
    } else {
        throw std::runtime_error("IAsset to Equity Cast Error!");
    }
}
}  // namespace finapp
