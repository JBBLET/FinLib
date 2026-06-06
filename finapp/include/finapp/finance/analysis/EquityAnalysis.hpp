// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>

#include "finapp/finance/analysis/IAssetAnalysis.hpp"
#include "finapp/finance/asset/Equity.hpp"
#include "finlib/session/TimeSeriesSession.hpp"

namespace finapp {
class AssetService;
}

namespace finance::analysis {

class EquityAnalysis : public IAssetAnalysis {
 public:
    EquityAnalysis(std::shared_ptr<finance::Equity> equity, std::shared_ptr<::analysis::TimeSeriesSession> session,
                   std::shared_ptr<finapp::AssetService> assetService);

    ~EquityAnalysis() = default;

 private:
    std::shared_ptr<finapp::AssetService> assetService_;
};

}  // namespace finance::analysis
