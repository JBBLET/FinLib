// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>
#include <unordered_map>

#include "finapp/common/logger/ILogger.hpp"
#include "finapp/finance/analysis/IAssetAnalysis.hpp"
#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/common/AssetId.hpp"
#include "finapp/service/AssetService.hpp"
#include "finapp/service/analysisService/IAssetAnalysisService.hpp"
#include "finlib/common/FinlibTypes.hpp"

using ts::Timestamp;
using ts::TimestampsPtr;

namespace finapp {

class PortfolioAnalysisService;

class AssetAnalysisService {
 public:
    AssetAnalysisService(std::shared_ptr<AssetService> assetService,
                         std::unordered_map<finance::AssetType, std::shared_ptr<IAssetAnalysisService>> services,
                         finapp::logging::ILogger* logger = nullptr);

    // Full factory — creates a session then wraps it into a typed IAssetAnalysis.
    std::shared_ptr<finance::analysis::IAssetAnalysis> createAnalysis(const finance::AssetId& id, Timestamp startMs,
                                                                      Timestamp endMs, Timestamp frequencyMs);
    std::shared_ptr<finance::analysis::IAssetAnalysis> createAnalysis(const finance::AssetId& id,
                                                                      TimestampsPtr timestamps);

    // Session-reuse factory — wraps an existing session without fetching.
    std::shared_ptr<finance::analysis::IAssetAnalysis> createAnalysisFromSession(
        const finance::AssetId& assetId,  //
        std::shared_ptr<ts::analysis::TimeSeriesSession> session);

 private:
    std::shared_ptr<AssetService> assetService_;
    std::unordered_map<finance::AssetType, std::shared_ptr<IAssetAnalysisService>> services_;
    std::unique_ptr<finapp::logging::ILogger> logger_;
};

}  // namespace finapp
