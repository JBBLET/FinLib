// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>

#include "finapp/finance/analysis/IAssetAnalysis.hpp"
#include "finapp/service/AssetService.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"
#include "finlib/session/TimeSeriesSession.hpp"

namespace finapp {

class IAssetAnalysisService {
 public:
    virtual ~IAssetAnalysisService() = default;

    virtual std::shared_ptr<finance::analysis::IAssetAnalysis> createAnalysis(
        std::shared_ptr<const finance::IAsset> asset,          //
        std::shared_ptr<analysis::TimeSeriesSession> session,  //
        std::shared_ptr<AssetService> AssetService) = 0;

 protected:
    std::shared_ptr<TimeSeriesService> getAssetTimeSeriesService(std::shared_ptr<AssetService> assetService) {
        return assetService->timeSeriesService_;
    }
};

}  // namespace finapp
