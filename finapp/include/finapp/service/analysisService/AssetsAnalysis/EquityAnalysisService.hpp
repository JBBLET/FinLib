// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "finapp/service/AssetService.hpp"
#include "finapp/service/analysisService/IAssetAnalysisService.hpp"

namespace finapp {

class EquityAnalysisService : public IAssetAnalysisService {
 public:
    explicit EquityAnalysisService(std::shared_ptr<AssetService> assetService);

    std::shared_ptr<finance::analysis::IAssetAnalysis> createAnalysis(
        std::shared_ptr<const finance::IAsset> asset, std::shared_ptr<TimeSeriesService> timeSeriesService,
        int64_t startMs, int64_t endMs, int64_t frequencyMs) override;

    std::shared_ptr<finance::analysis::IAssetAnalysis> createAnalysis(
        std::shared_ptr<const finance::IAsset> asset, std::shared_ptr<TimeSeriesService> timeSeriesService,
        std::shared_ptr<std::vector<int64_t>> timestamps) override;

 private:
    std::shared_ptr<AssetService> assetService_;
};

}  // namespace finapp
