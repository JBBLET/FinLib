// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/service/AssetService.hpp"
#include "finapp/service/analysisService/IAssetAnalysisService.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"

namespace finapp {

class AssetAnalysisService {
    friend class PortfolioAnalysisService;
    std::unordered_map<::finance::AssetType, std::shared_ptr<IAssetAnalysisService>> AnalysisServices_;
    std::shared_ptr<AssetService> assetService_;

 public:
    std::shared_ptr<IAssetAnalysisService> createAnalysis(const ::finance::AssetId&, int64_t startMs, int64_t endMs,
                                                          int64_t frequencyMs);
};

}  // namespace finapp
