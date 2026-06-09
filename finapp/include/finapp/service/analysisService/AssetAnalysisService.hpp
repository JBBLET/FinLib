// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "finapp/finance/analysis/IAssetAnalysis.hpp"
#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/common/AssetId.hpp"
#include "finapp/service/AssetService.hpp"
#include "finapp/service/analysisService/IAssetAnalysisService.hpp"

namespace finapp {

class PortfolioAnalysisService;

class AssetAnalysisService {
    friend class PortfolioAnalysisService;

 public:
    AssetAnalysisService(
        std::shared_ptr<AssetService> assetService,
        std::unordered_map<finance::AssetType, std::shared_ptr<IAssetAnalysisService>> services);

    std::shared_ptr<finance::analysis::IAssetAnalysis> createAnalysis(
        const finance::AssetId& id,
        int64_t startMs, int64_t endMs, int64_t frequencyMs);

    std::shared_ptr<finance::analysis::IAssetAnalysis> createAnalysis(
        const finance::AssetId& id,
        std::shared_ptr<std::vector<int64_t>> timestamps);

 private:
    std::shared_ptr<AssetService> assetService_;
    std::unordered_map<finance::AssetType, std::shared_ptr<IAssetAnalysisService>> services_;
};

}  // namespace finapp
