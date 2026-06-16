// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>

#include "finapp/service/AssetService.hpp"
#include "finapp/service/analysisService/IAssetAnalysisService.hpp"
#include "finlib/session/TimeSeriesSession.hpp"

namespace finapp {

class EquityAnalysisService : public IAssetAnalysisService {
 public:
    explicit EquityAnalysisService(std::shared_ptr<AssetService> assetService);

    std::shared_ptr<finance::analysis::IAssetAnalysis> createAnalysisFromSession(
        std::shared_ptr<const finance::IAsset> asset,  //
        std::shared_ptr<::analysis::TimeSeriesSession> session) override;

 private:
    std::shared_ptr<AssetService> assetService_;
};

}  // namespace finapp
