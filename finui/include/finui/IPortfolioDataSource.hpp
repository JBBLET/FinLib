// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <string>
#include <vector>

namespace finui {

struct PositionRow {
    std::string ticker;
    double quantity = 0.0;
    double value    = 0.0;
    double weight   = 0.0;  // [0.0, 1.0]
};

struct PortfolioSummary {
    std::string id;
    std::string name;
    std::string baseCurrency;
    double totalValue  = 0.0;
    double cashBalance = 0.0;
    std::vector<PositionRow> positions;
};

class IPortfolioDataSource {
 public:
    virtual ~IPortfolioDataSource() = default;
    virtual std::vector<std::string> listPortfolioIds() = 0;
    virtual PortfolioSummary loadSummary(const std::string& portfolioId) = 0;
};

}  // namespace finui
