// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/finance/portfolio/Transaction.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace finance {

namespace {
const std::unordered_map<std::string, TransactionType> nameToTransactionTypeMap = {
    {"Buy", TransactionType::Buy},
    {"Sell", TransactionType::Sell},
    {"Deposit", TransactionType::Deposit},
    {"Withdrawal", TransactionType::Withdrawal},
    {"Dividend", TransactionType::Dividend},
    {"Split", TransactionType::Split}};
}  // namespace

std::string toString(const TransactionType& transactionType) {
    switch (transactionType) {
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

TransactionType transactionTypeFromString(const std::string& type) {
    auto it = nameToTransactionTypeMap.find(type);
    if (it == nameToTransactionTypeMap.end()) throw std::runtime_error("Unsupported transaction type: '" + type + "'");
    return it->second;
}

int transactionTypePriority(TransactionType t) {
    switch (t) {
        case TransactionType::Deposit:
            return 0;
        case TransactionType::Dividend:
            return 1;
        case TransactionType::Buy:
            return 2;
        case TransactionType::Sell:
            return 2;
        case TransactionType::Withdrawal:
            return 3;
        case TransactionType::Split:
            return 4;
        default:
            return 5;
    }
}

}  // namespace finance
