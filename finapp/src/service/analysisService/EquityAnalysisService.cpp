// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/service/analysisService/AssetsAnalysis/EquityAnalysisService.hpp"

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "finapp/finance/analysis/EquityAnalysis.hpp"
#include "finapp/finance/asset/Equity.hpp"
#include "finlib/session/TimeSeriesSession.hpp"

namespace finapp {

EquityAnalysisService::EquityAnalysisService(std::shared_ptr<AssetService> assetService)
    : assetService_{std::move(assetService)} {}

std::shared_ptr<finance::analysis::IAssetAnalysis> EquityAnalysisService::createAnalysis(
    std::shared_ptr<const finance::IAsset> asset, std::shared_ptr<TimeSeriesService> timeSeriesService, int64_t startMs,
    int64_t endMs, int64_t frequencyMs) {
    auto equity = std::dynamic_pointer_cast<const finance::Equity>(asset);
    if (!equity) throw std::runtime_error("EquityAnalysisService: asset is not an Equity");
    auto session = std::make_shared<::analysis::TimeSeriesSession>(
        std::move(timeSeriesService), asset->priceSeriesId(), startMs, endMs, frequencyMs);
    return std::make_shared<finance::analysis::EquityAnalysis>(
        std::const_pointer_cast<finance::Equity>(equity), std::move(session), assetService_);
}

std::shared_ptr<finance::analysis::IAssetAnalysis> EquityAnalysisService::createAnalysis(
    std::shared_ptr<const finance::IAsset> asset, std::shared_ptr<TimeSeriesService> timeSeriesService,
    std::shared_ptr<std::vector<int64_t>> timestamps) {
    auto equity = std::dynamic_pointer_cast<const finance::Equity>(asset);
    if (!equity) throw std::runtime_error("EquityAnalysisService: asset is not an Equity");
    auto session = std::make_shared<::analysis::TimeSeriesSession>(
        std::move(timeSeriesService), asset->priceSeriesId(), std::move(timestamps));
    return std::make_shared<finance::analysis::EquityAnalysis>(
        std::const_pointer_cast<finance::Equity>(equity), std::move(session), assetService_);
}

}  // namespace finapp
