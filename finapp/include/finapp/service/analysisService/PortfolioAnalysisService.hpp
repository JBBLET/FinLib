// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "finapp/finance/analysis/IAssetAnalysis.hpp"
#include "finapp/finance/analysis/PortfolioAnalysis.hpp"
#include "finapp/service/analysisService/AssetAnalysisService.hpp"
#include "finlib/session/MultiTimeSeriesSession.hpp"

namespace finance {
class Portfolio;
}

namespace finapp {

class PortfolioAnalysisService {
 public:
    explicit PortfolioAnalysisService(std::shared_ptr<AssetAnalysisService> assetAnalysisService);

    // Full portfolio analysis: per-asset sessions wired into a MultiTimeSeriesSession.
    // NavMode is chosen automatically:
    //   - TargetWeighted if portfolio.targetAllocations() is non-empty.
    //   - QuantityBased otherwise (snapshot positions).
    // For proper historical NAV from transaction replay, call
    // PortfolioAnalysis::setNavTimeSeries() with the result of PortfolioService::getNavTimeSeries().
    std::shared_ptr<finance::analysis::PortfolioAnalysis> createPortfolioAnalysis(
        const finance::Portfolio& portfolio, int64_t startMs, int64_t endMs, int64_t frequencyMs);

    std::shared_ptr<finance::analysis::PortfolioAnalysis> createPortfolioAnalysis(
        const finance::Portfolio& portfolio, std::shared_ptr<std::vector<int64_t>> timestamps);

 private:
    std::shared_ptr<AssetAnalysisService> assetAnalysisService_;

    // Returns one IAssetAnalysis per position. Internal step used by createPortfolioAnalysis.
    std::vector<std::shared_ptr<finance::analysis::IAssetAnalysis>> buildAssetAnalyses_(
        const finance::Portfolio& portfolio, int64_t startMs, int64_t endMs, int64_t frequencyMs);

    std::vector<std::shared_ptr<finance::analysis::IAssetAnalysis>> buildAssetAnalyses_(
        const finance::Portfolio& portfolio, std::shared_ptr<std::vector<int64_t>> timestamps);

    // Selects NavMode and builds the weight map from the portfolio.
    static std::pair<finance::analysis::NavMode, std::unordered_map<std::string, double>> resolveNavWeights_(
        const finance::Portfolio& portfolio);

    // Wires analyses into a MultiTimeSeriesSession and constructs PortfolioAnalysis.
    static std::shared_ptr<finance::analysis::PortfolioAnalysis> assemble_(
        const finance::Portfolio& portfolio,
        std::vector<std::shared_ptr<finance::analysis::IAssetAnalysis>> analyses);
};

}  // namespace finapp
