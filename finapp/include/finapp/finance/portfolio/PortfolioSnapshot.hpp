// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "finapp/finance/common/AssetId.hpp"
#include "finapp/finance/common/Currency.hpp"

using Timestamp = int64_t;
namespace finance {

struct SnapshotPosition {
    AssetId assetId;
    double quantity;
};

struct PortfolioSnapshot {
    std::string name;
    Currency baseCurrency;
    int64_t timestampMs;
    std::string portfolioId;
    std::vector<SnapshotPosition> positions;
    std::unordered_map<Currency, double> cashBalances;
};

struct PortfolioOverviewAtTs {
    Timestamp timestamp;
    double totalValue;
    std::unordered_map<std::string, double> weights;
};
}  // namespace finance
