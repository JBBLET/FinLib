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
    // Starts clean — exposes the raw price series only. Analysis features
    // (returns, metrics, ...) are installed explicitly via installFeature(),
    // so they are treated exactly like any client-supplied custom feature.
    EquityAnalysis(std::shared_ptr<const finance::Equity> equity,
                   std::shared_ptr<ts::analysis::TimeSeriesSession> session,
                   std::shared_ptr<finapp::AssetService> assetService);

    ~EquityAnalysis() = default;

 private:
    std::shared_ptr<finapp::AssetService> assetService_;
};

}  // namespace finance::analysis
