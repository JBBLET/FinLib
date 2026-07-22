// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/service/analysisService/AssetAnalysisService.hpp"

#include <memory>
#include <unordered_map>
#include <utility>

#include "finapp/common/Exception.hpp"
#include "finapp/common/logger/PrefixedLogger.hpp"
#include "finapp/service/AssetService.hpp"
#include "finlib/common/FinlibTypes.hpp"

namespace finapp {

AssetAnalysisService::AssetAnalysisService(
    std::shared_ptr<AssetService> assetService,
    std::unordered_map<finance::AssetType, std::shared_ptr<IAssetAnalysisService>> services,
    finapp::logging::ILogger* logger)
    : assetService_{std::move(assetService)},
      services_{std::move(services)},
      logger_{finapp::logging::PrefixedLogger::wrap(logger, "AssetAnalysisService")} {}

std::shared_ptr<finance::analysis::IAssetAnalysis> AssetAnalysisService::createAnalysis(const finance::AssetId& id,
                                                                                        Timestamp startMs,
                                                                                        Timestamp endMs,
                                                                                        Timestamp frequencyMs) {
    if (logger_)
        logger_->write(
            finapp::logging::Level::Debug,
            "createAnalysis '" + id.ticker + "' [" + std::to_string(startMs) + ".." + std::to_string(endMs) + "]");
    auto asset = assetService_->load(id);
    auto session = assetService_->createSession(id, startMs, endMs, frequencyMs);
    auto it = services_.find(id.type);
    if (it == services_.end()) throw finapp::Exception("No analysis service for asset type");
    return it->second->createAnalysisFromSession(asset, std::move(session));
}

std::shared_ptr<finance::analysis::IAssetAnalysis> AssetAnalysisService::createAnalysis(const finance::AssetId& id,
                                                                                        TimestampsPtr timestamps) {
    if (logger_)
        logger_->write(
            finapp::logging::Level::Debug,
            "createAnalysis '" + id.ticker + "' (custom grid, size=" + std::to_string(timestamps->size()) + ")");
    auto asset = assetService_->load(id);
    auto session = assetService_->createSession(id, timestamps);
    auto it = services_.find(id.type);
    if (it == services_.end()) throw finapp::Exception("No analysis service for asset type");
    return it->second->createAnalysisFromSession(asset, std::move(session));
}

std::shared_ptr<finance::analysis::IAssetAnalysis> AssetAnalysisService::createAnalysisFromSession(
    const finance::AssetId& assetId,  //
    std::shared_ptr<TimeSeriesSession> session) {
    if (logger_) logger_->write(finapp::logging::Level::Debug, "createAnalysisFromSession '" + assetId.ticker + "'");
    auto asset = assetService_->load(assetId);
    auto it = services_.find(assetId.type);
    if (it == services_.end()) throw finapp::Exception("No analysis service for asset type");
    return it->second->createAnalysisFromSession(asset, std::move(session));
}

}  // namespace finapp
