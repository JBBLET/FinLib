// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/service/analysisService/PortfolioAnalysisService.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finapp/finance/analysis/PortfolioAnalysis.hpp"
#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finlib/session/MultiTimeSeriesSession.hpp"

namespace finapp {

PortfolioAnalysisService::PortfolioAnalysisService(std::shared_ptr<AssetAnalysisService> assetAnalysisService)
    : assetAnalysisService_{std::move(assetAnalysisService)} {}

// ---------------------------------------------------------------------------
// Public factory
// ---------------------------------------------------------------------------
std::shared_ptr<finance::analysis::PortfolioAnalysis> PortfolioAnalysisService::createPortfolioAnalysis(
    const finance::Portfolio& portfolio, int64_t startMs, int64_t endMs, int64_t frequencyMs) {
    return assemble_(portfolio, buildAssetAnalyses_(portfolio, startMs, endMs, frequencyMs));
}

std::shared_ptr<finance::analysis::PortfolioAnalysis> PortfolioAnalysisService::createPortfolioAnalysis(
    const finance::Portfolio& portfolio, std::shared_ptr<std::vector<int64_t>> timestamps) {
    return assemble_(portfolio, buildAssetAnalyses_(portfolio, std::move(timestamps)));
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
std::vector<std::shared_ptr<finance::analysis::IAssetAnalysis>> PortfolioAnalysisService::buildAssetAnalyses_(
    const finance::Portfolio& portfolio, int64_t startMs, int64_t endMs, int64_t frequencyMs) {
    std::vector<std::shared_ptr<finance::analysis::IAssetAnalysis>> result;
    result.reserve(portfolio.positions().size());
    for (const auto& pos : portfolio.positions())
        result.push_back(assetAnalysisService_->createAnalysis(pos.assetId, startMs, endMs, frequencyMs));
    return result;
}

std::vector<std::shared_ptr<finance::analysis::IAssetAnalysis>> PortfolioAnalysisService::buildAssetAnalyses_(
    const finance::Portfolio& portfolio, std::shared_ptr<std::vector<int64_t>> timestamps) {
    std::vector<std::shared_ptr<finance::analysis::IAssetAnalysis>> result;
    result.reserve(portfolio.positions().size());
    // Shared_ptr copy each iteration — all sessions reference the same timestamp grid.
    for (const auto& pos : portfolio.positions())
        result.push_back(assetAnalysisService_->createAnalysis(pos.assetId, timestamps));
    return result;
}

std::pair<finance::analysis::NavMode, std::unordered_map<std::string, double>>
PortfolioAnalysisService::resolveNavWeights_(const finance::Portfolio& portfolio) {
    using NavMode = finance::analysis::NavMode;

    if (!portfolio.targetAllocations().empty()) {
        // Target-weight portfolio: use declared allocations.
        // Normalize in case they don't sum exactly to 1.0.
        std::unordered_map<std::string, double> weights;
        weights.reserve(portfolio.targetAllocations().size());

        double total = 0.0;
        for (const auto& alloc : portfolio.targetAllocations()) total += alloc.weight;
        const double norm = (total > 1e-12) ? 1.0 / total : 1.0;

        for (const auto& alloc : portfolio.targetAllocations()) weights[alloc.assetId.ticker] = alloc.weight * norm;

        return {NavMode::TargetWeighted, std::move(weights)};
    }

    // Transaction-based portfolio: use snapshot quantities.
    // NAV(t) = sum(q_i * price_i(t)) — accurate only at snapshot time.
    // Call PortfolioAnalysis::setNavTimeSeries() to override with replayed transaction data.
    std::unordered_map<std::string, double> quantities;
    quantities.reserve(portfolio.positions().size());
    for (const auto& pos : portfolio.positions()) quantities[pos.assetId.ticker] = pos.quantity;

    return {NavMode::QuantityBased, std::move(quantities)};
}

std::shared_ptr<finance::analysis::PortfolioAnalysis> PortfolioAnalysisService::assemble_(
    const finance::Portfolio& portfolio, std::vector<std::shared_ptr<finance::analysis::IAssetAnalysis>> analyses) {
    auto multiSession = std::make_unique<::analysis::MultiTimeSeriesSession>();
    for (const auto& a : analyses) multiSession->addSession(a->asset()->ticker(), a->sessionPtr());

    auto [navMode, navWeights] = resolveNavWeights_(portfolio);

    return std::make_shared<finance::analysis::PortfolioAnalysis>(
        portfolio, std::move(multiSession), std::move(analyses), std::move(navWeights), navMode);
}

}  // namespace finapp
