// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "finapp/finance/analysis/IAssetAnalysis.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"

namespace finapp {

class IAssetAnalysisService {
 public:
    virtual ~IAssetAnalysisService() = default;

    virtual std::shared_ptr<finance::analysis::IAssetAnalysis> createAnalysis(
        std::shared_ptr<const finance::IAsset> asset,
        std::shared_ptr<TimeSeriesService> timeSeriesService,
        int64_t startMs, int64_t endMs, int64_t frequencyMs) = 0;

    virtual std::shared_ptr<finance::analysis::IAssetAnalysis> createAnalysis(
        std::shared_ptr<const finance::IAsset> asset,
        std::shared_ptr<TimeSeriesService> timeSeriesService,
        std::shared_ptr<std::vector<int64_t>> timestamps) = 0;
};

}  // namespace finapp
