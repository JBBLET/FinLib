// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>
#include <optional>

#include "finapp/finance/analysis/IAssetAnalysis.hpp"
#include "finapp/finance/asset/Equity.hpp"
#include "finlib/analysis/MetricHandle.hpp"
#include "finlib/session/TimeSeriesSession.hpp"

namespace finapp {
class AssetService;
}

namespace finance::analysis {

class EquityAnalysis : public IAssetAnalysis {
 public:
    EquityAnalysis(std::shared_ptr<const finance::Equity> equity,
                   std::shared_ptr<::analysis::TimeSeriesSession> session,
                   std::shared_ptr<finapp::AssetService> assetService);

    ~EquityAnalysis() = default;

 private:
    std::shared_ptr<finapp::AssetService> assetService_;

    std::optional<::analysis::MetricHandle<double>> totalReturnHandle_;
    std::optional<::analysis::MetricHandle<double>> sharpeHandle_;
};

}  // namespace finance::analysis
