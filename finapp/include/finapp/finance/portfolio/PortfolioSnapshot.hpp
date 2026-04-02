// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "finapp/finance/common/AssetId.hpp"
#include "finapp/finance/common/Currency.hpp"

namespace finance {

struct SnapshotPosition {
    AssetId assetId;
    double quantity;
};

struct PortfolioSnapshot {
    int64_t timestampMs;
    std::string portfolioId;
    std::vector<SnapshotPosition> positions;
    std::unordered_map<Currency, double> cashBalances;
};

}  // namespace finance
