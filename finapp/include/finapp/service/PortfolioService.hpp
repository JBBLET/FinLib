// Copyright (c) 2026 JBBLET. All Rights Reserved.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "finapp/data/repository/IPortfolioRepository.hpp"
#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finlib/core/TimeSeries.hpp"

namespace finance {
class PortfolioService {
 public:
    PortfolioService(std::shared_ptr<IPortfolioRepository> portfolioRepository, std::shared_ptr<IAssetService>);

    Portfolio load(const std::string& portfolioId);

    void save(const Portfolio& portfolio, int64_t timestampsMs);

    TimeSeries valueSeries(const std::string& portfolioId, int64_t startMs, int64_t endMs, int64_t frequencyMs);

    // TODO(JBBLET) Change to shared_ptr towards TimeSeries View ? save memory if two portfolio share the same view for
    // their analysis
    //  small performance gain ?
    std::unordered_map<std::string, TimeSeries> weightSeries(const std::string& portfolioId, int64_t startMs,
                                                             int64_t endMs, int64_t frequencyMs);

 private:
    std::shared_ptr<IPortfolioRepository> portfolioRepository_;
    std::shared_ptr<IAssetService> assetService_;

    void recomputeAndCache_(const Portfolio& portfolio, int64_t fromMs, int64_t toMs, int64_t frequencyMs);
};
}  // namespace finance
