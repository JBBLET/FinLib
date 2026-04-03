// Copyright (c) 2026 JBBLET. All Rights Reserved.

#pragma once
#include <string>

#include "finapp/finance/asset/IAsset.hpp"
#include "finapp/finance/common/Currency.hpp"
namespace finance {
enum class TransactionType : uint8_t {
    Buy,
    Sell,
    Deposit,
    Withdrawal,
    Dividend,
    Split,
};

struct Transaction {
    int64_t timestampsMs;
    TransactionType type;
    AssetType assetType;
    std::string assetTicker;
    double quantity;
    double pricePerUnit;
    double fees;
    Currency SettlementCurrency;
};
}  // namespace finance
