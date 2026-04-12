// Copyright (c) 2026 JBBLET. All Rights Reserved.

#pragma once
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "finapp/finance/asset/AssetType.hpp"
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

inline std::string toString(const TransactionType& TransactionType) {
    switch (TransactionType) {
        case TransactionType::Buy:
            return "Buy";
        case TransactionType::Sell:
            return "Sell";
        case TransactionType::Deposit:
            return "Deposit";
        case TransactionType::Withdrawal:
            return "Withdrawal";
        case TransactionType::Dividend:
            return "Dividend";
        case TransactionType::Split:
            return "Split";
        default:
            throw std::runtime_error("Non supported Transaction type");
    }
}
static std::unordered_map<std::string, TransactionType> TransactionTypeStringMap = {
    {"Buy", TransactionType::Buy},           {"Sell", TransactionType::Sell},
    {"Deposit", TransactionType::Deposit},   {"Withdrawal", TransactionType::Withdrawal},
    {"Dividend", TransactionType::Dividend}, {"Split", TransactionType::Split}};

inline TransactionType transactionTypeFromString(const std::string& type) { return TransactionTypeStringMap[type]; }

struct Transaction {
    int64_t timestampsMs;
    TransactionType type;
    AssetType assetType;
    std::string assetTicker;
    double quantity;
    double pricePerUnit;
    double fees;
    Currency settlementCurrency;
    bool operator==(const Transaction& other) const {
        return timestampsMs == other.timestampsMs && type == other.type && assetType == other.assetType &&
               assetTicker == other.assetTicker;
    }
};
}  // namespace finance

namespace std {
// TODO(JBBLET) Change to just a custom hasher to  not pollute the std namespace ??
template <>
struct hash<finance::Transaction> {
    size_t operator()(const finance::Transaction& transaction) const {
        std::size_t result = 12;
        result = result * 17 + std::hash<int64_t>()(transaction.timestampsMs);
        result = result * 17 + std::hash<finance::TransactionType>()(transaction.type);
        result = result * 17 + std::hash<finance::AssetType>()(transaction.assetType);
        result = result * 17 + std::hash<std::string>()(transaction.assetTicker);
        return result;
    }
};
}  // namespace std
