// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/service/analysisService/AssetAnalysisService.hpp"

#include <memory>
#include <unordered_map>
#include <utility>

#include "finapp/common/Error.hpp"
#include "finapp/common/Log.hpp"
#include "finapp/service/AssetService.hpp"
#include "finlib/common/FinlibTypes.hpp"

namespace finapp {

AssetAnalysisService::AssetAnalysisService(
    std::shared_ptr<AssetService> assetService,
    std::unordered_map<finance::AssetType, std::shared_ptr<IAssetAnalysisService>> services)
    : assetService_{std::move(assetService)},
      services_{std::move(services)} {}

std::shared_ptr<finance::analysis::IAssetAnalysis> AssetAnalysisService::createAnalysis(const finance::AssetId& id,
                                                                                        Timestamp startMs,
                                                                                        Timestamp endMs,
                                                                                        Timestamp frequencyMs) {
    logging::debug("createAnalysis '{}' [{}..{}]", id.ticker, startMs, endMs);
    auto asset = assetService_->load(id);
    auto session = assetService_->createSession(id, startMs, endMs, frequencyMs);
    auto it = services_.find(id.type);
    ensure(it != services_.end(), "No analysis service for asset type");
    return it->second->createAnalysisFromSession(asset, std::move(session));
}

std::shared_ptr<finance::analysis::IAssetAnalysis> AssetAnalysisService::createAnalysis(const finance::AssetId& id,
                                                                                        TimestampsPtr timestamps) {
    logging::debug("createAnalysis '{}' (custom grid, size={})", id.ticker, timestamps->size());
    auto asset = assetService_->load(id);
    auto session = assetService_->createSession(id, timestamps);
    auto it = services_.find(id.type);
    ensure(it != services_.end(), "No analysis service for asset type");
    return it->second->createAnalysisFromSession(asset, std::move(session));
}

std::shared_ptr<finance::analysis::IAssetAnalysis> AssetAnalysisService::createAnalysisFromSession(
    const finance::AssetId& assetId,  //
    std::shared_ptr<TimeSeriesSession> session) {
    logging::debug("createAnalysisFromSession '{}'", assetId.ticker);
    auto asset = assetService_->load(assetId);
    auto it = services_.find(assetId.type);
    ensure(it != services_.end(), "No analysis service for asset type");
    return it->second->createAnalysisFromSession(asset, std::move(session));
}

}  // namespace finapp
