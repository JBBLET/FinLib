// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finapp/common/logger/ILogger.hpp"
#include "finapp/finance/analysis/IAssetAnalysis.hpp"
#include "finapp/finance/analysis/PortfolioAnalysis.hpp"
#include "finapp/service/analysisService/AssetAnalysisService.hpp"
#include "finlib/common/FinlibTypes.hpp"

namespace finance {
class Portfolio;
}

using ts::Timestamp;
using ts::Timestamps;

namespace finapp {

class PortfolioAnalysisService {
 public:
    explicit PortfolioAnalysisService(std::shared_ptr<AssetAnalysisService> assetAnalysisService,
                                      finapp::logging::ILogger* logger = nullptr);

    std::shared_ptr<finance::analysis::PortfolioAnalysis> createPortfolioAnalysis(const finance::Portfolio& portfolio,
                                                                                  Timestamp startMs, Timestamp endMs,
                                                                                  Timestamp frequencyMs);

    std::shared_ptr<finance::analysis::PortfolioAnalysis> createPortfolioAnalysis(
        const finance::Portfolio& portfolio, std::shared_ptr<Timestamps> timestamps);

 private:
    std::shared_ptr<AssetAnalysisService> assetAnalysisService_;
    std::unique_ptr<finapp::logging::ILogger> logger_;

    std::vector<std::shared_ptr<finance::analysis::IAssetAnalysis>> buildAssetAnalyses_(
        const finance::Portfolio& portfolio, Timestamp startMs, Timestamp endMs, Timestamp frequencyMs);

    std::vector<std::shared_ptr<finance::analysis::IAssetAnalysis>> buildAssetAnalyses_(
        const finance::Portfolio& portfolio, std::shared_ptr<Timestamps> timestamps);

    static std::pair<finance::analysis::NavMode, std::unordered_map<std::string, double>> resolveNavWeights_(
        const finance::Portfolio& portfolio);

    static std::shared_ptr<finance::analysis::PortfolioAnalysis> assemble_(
        const finance::Portfolio& portfolio, std::vector<std::shared_ptr<finance::analysis::IAssetAnalysis>> analyses);
};

}  // namespace finapp
