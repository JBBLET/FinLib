// Copyright (c) 2026 JBBLET. All Rights Reserved.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "finapp/data/repository/IPortfolioRepository.hpp"
#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"
#include "finapp/service/AssetService.hpp"
#include "finapp/service/FXService.hpp"
#include "finlib/core/TimeSeries.hpp"

namespace finance {

class PortfolioService {
 public:
    PortfolioService(std::shared_ptr<IPortfolioRepository> portfolioRepository,
                     std::shared_ptr<AssetService> assetService, std::shared_ptr<FXService> fxService);

    // Reconstruct from snapshot + transactions
    Portfolio load(const std::string& portfolioId);

    // Persist snapshot + append new transactions
    void save(const Portfolio& portfolio, int64_t timestampMs);

    // Market-data-dependent computations (fetches prices/FX internally)
    double totalValue(const Portfolio& portfolio, int64_t timestampMs);
    std::unordered_map<std::string, double> weights(const Portfolio& portfolio, int64_t timestampMs);
    std::vector<Transaction> rebalance(const Portfolio& portfolio, int64_t timestampMs);

    // Derived TimeSeries over a range
    TimeSeries valueSeries(const std::string& portfolioId, int64_t startMs, int64_t endMs, int64_t frequencyMs);
    std::unordered_map<std::string, TimeSeries> weightSeries(const std::string& portfolioId, int64_t startMs,
                                                              int64_t endMs, int64_t frequencyMs);

 private:
    std::shared_ptr<IPortfolioRepository> portfolioRepository_;
    std::shared_ptr<AssetService> assetService_;
    std::shared_ptr<FXService> fxService_;

    void recomputeAndCache_(const Portfolio& portfolio, int64_t fromMs, int64_t toMs, int64_t frequencyMs);
};
}  // namespace finance
