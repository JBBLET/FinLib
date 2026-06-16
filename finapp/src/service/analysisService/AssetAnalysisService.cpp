// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/service/analysisService/AssetAnalysisService.hpp"

#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "finapp/service/AssetService.hpp"
#include "finlib/common/FinlibTypes.hpp"

namespace finapp {

AssetAnalysisService::AssetAnalysisService(
    std::shared_ptr<AssetService> assetService,
    std::unordered_map<finance::AssetType, std::shared_ptr<IAssetAnalysisService>> services)
    : assetService_{std::move(assetService)}, services_{std::move(services)} {}

std::shared_ptr<finance::analysis::IAssetAnalysis> AssetAnalysisService::createAnalysis(
    const finance::AssetId& id, Timestamp startMs, Timestamp endMs, Timestamp frequencyMs) {
    auto asset = assetService_->load(id);
    auto session = assetService_->createSession(id, startMs, endMs, frequencyMs);
    auto it = services_.find(id.type);
    if (it == services_.end()) throw std::runtime_error("No analysis service for asset type");
    return it->second->createAnalysisFromSession(asset, std::move(session));
}

std::shared_ptr<finance::analysis::IAssetAnalysis> AssetAnalysisService::createAnalysis(
    const finance::AssetId& id, TimestampsPtr timestamps) {
    auto asset = assetService_->load(id);
    auto session = assetService_->createSession(id, timestamps);
    auto it = services_.find(id.type);
    if (it == services_.end()) throw std::runtime_error("No analysis service for asset type");
    return it->second->createAnalysisFromSession(asset, std::move(session));
}

std::shared_ptr<finance::analysis::IAssetAnalysis> AssetAnalysisService::createAnalysisFromSession(
    const finance::AssetId& assetId,  //
    std::shared_ptr<::analysis::TimeSeriesSession> session) {
    auto asset = assetService_->load(assetId);
    auto it = services_.find(assetId.type);
    if (it == services_.end()) throw std::runtime_error("No analysis service for asset type");
    return it->second->createAnalysisFromSession(asset, std::move(session));
}

}  // namespace finapp
