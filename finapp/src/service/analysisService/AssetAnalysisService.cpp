// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/service/analysisService/AssetAnalysisService.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

#include "finapp/service/AssetService.hpp"

namespace finapp {

AssetAnalysisService::AssetAnalysisService(
    std::shared_ptr<AssetService> assetService,
    std::unordered_map<finance::AssetType, std::shared_ptr<IAssetAnalysisService>> services)
    : assetService_{std::move(assetService)}, services_{std::move(services)} {}

std::shared_ptr<finance::analysis::IAssetAnalysis> AssetAnalysisService::createAnalysis(
    const finance::AssetId& id, int64_t startMs, int64_t endMs, int64_t frequencyMs) {
    auto asset = assetService_->load(id);
    auto tss = assetService_->timeSeriesService_;  // friend access
    auto it = services_.find(id.type);
    if (it == services_.end())
        throw std::runtime_error("No analysis service registered for asset type");
    return it->second->createAnalysis(asset, tss, startMs, endMs, frequencyMs);
}

std::shared_ptr<finance::analysis::IAssetAnalysis> AssetAnalysisService::createAnalysis(
    const finance::AssetId& id, std::shared_ptr<std::vector<int64_t>> timestamps) {
    auto asset = assetService_->load(id);
    auto tss = assetService_->timeSeriesService_;  // friend access
    auto it = services_.find(id.type);
    if (it == services_.end())
        throw std::runtime_error("No analysis service registered for asset type");
    return it->second->createAnalysis(asset, tss, std::move(timestamps));
}

}  // namespace finapp
