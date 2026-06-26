// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>

#include "finapp/finance/analysis/IAssetAnalysis.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finlib/session/TimeSeriesSession.hpp"

using ts::analysis::TimeSeriesSession;

namespace finapp {

class IAssetAnalysisService {
 public:
    virtual ~IAssetAnalysisService() = default;

    virtual std::shared_ptr<finance::analysis::IAssetAnalysis> createAnalysisFromSession(
        std::shared_ptr<const finance::IAsset> asset,  //
        std::shared_ptr<TimeSeriesSession> session) = 0;
};
}  // namespace finapp
