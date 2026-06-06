// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/service/AnalysisService.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "finapp/finance/analysis/EquityAnalysis.hpp"
#include "finapp/finance/analysis/IAssetAnalysis.hpp"
#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/asset/Equity.hpp"
#include "finapp/finance/common/AssetId.hpp"
#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finapp/service/AssetService.hpp"
#include "finlib/session/TimeSeriesSession.hpp"

namespace finapp {

AnalysisService::AnalysisService(std::shared_ptr<AssetService> assetService,
                                 std::shared_ptr<TimeSeriesService> timeSeriesService)
    : assetService_{std::move(assetService)},
      timeSeriesService_{std::move(timeSeriesService)} {}

std::shared_ptr<finance::analysis::EquityAnalysis>
AnalysisService::createEquityAnalysis(const std::string& ticker,
                                      int64_t startMs, int64_t endMs, int64_t frequencyMs) {
    finance::AssetId id{finance::AssetType::Equity, ticker};
    auto asset = assetService_->load(id);

    auto equity = std::dynamic_pointer_cast<const finance::Equity>(asset);
    if (!equity)
        throw std::runtime_error("Asset '" + ticker + "' is not an Equity");

    auto session = std::make_shared<::analysis::TimeSeriesSession>(
        timeSeriesService_, equity->priceSeriesId(), startMs, endMs, frequencyMs);

    // const_pointer_cast: EquityAnalysis constructor takes shared_ptr<Equity> (non-const)
    // for storage in IAssetAnalysis::asset_. The asset data is immutable in practice.
    return std::make_shared<finance::analysis::EquityAnalysis>(
        std::const_pointer_cast<finance::Equity>(equity),
        std::move(session),
        assetService_);
}

std::vector<std::shared_ptr<finance::analysis::IAssetAnalysis>>
AnalysisService::createPortfolioAnalysis(const finance::Portfolio& portfolio,
                                         int64_t startMs, int64_t endMs, int64_t frequencyMs) {
    std::vector<std::shared_ptr<finance::analysis::IAssetAnalysis>> result;

    for (const auto& pos : portfolio.positions()) {
        if (pos.assetId.type != finance::AssetType::Equity) continue;
        result.push_back(createEquityAnalysis(pos.assetId.ticker, startMs, endMs, frequencyMs));
    }

    return result;
}

}  // namespace finapp
