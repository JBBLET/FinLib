// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "finlib/data/services/TimeSeriesService.hpp"

namespace finance {
class Portfolio;
}

namespace finance::analysis {
class IAssetAnalysis;
class EquityAnalysis;
}

namespace finapp {
class AssetService;
}

namespace finapp {

class AnalysisService {
 public:
    // Both services must share the same underlying TimeSeriesService / repository.
    AnalysisService(std::shared_ptr<AssetService> assetService,
                    std::shared_ptr<TimeSeriesService> timeSeriesService);

    std::shared_ptr<finance::analysis::EquityAnalysis>
    createEquityAnalysis(const std::string& ticker,
                         int64_t startMs, int64_t endMs, int64_t frequencyMs);

    // Creates one analysis object per position in the portfolio (equity positions only for now).
    std::vector<std::shared_ptr<finance::analysis::IAssetAnalysis>>
    createPortfolioAnalysis(const finance::Portfolio& portfolio,
                            int64_t startMs, int64_t endMs, int64_t frequencyMs);

 private:
    std::shared_ptr<AssetService>      assetService_;
    std::shared_ptr<TimeSeriesService> timeSeriesService_;
};

}  // namespace finapp
