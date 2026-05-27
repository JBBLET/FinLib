// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace finui {

inline const std::vector<std::string> currencies = {"USD", "EUR", "GBP", "JPY", "KRW", "CAD"};
inline const std::vector<std::string> transactionTypes = {"BUY", "SELL", "DEPOSIT", "WITHDRAWAL", "DIVIDEND", "SPLIT"};

struct PositionRow {
    std::string ticker;
    double quantity = 0.0;
    double value = 0.0;
    double weight = 0.0;
};

struct CashRow {
    std::string currency;
    double amount = 0.0;
};

struct TransactionRow {
    std::string id;
    int64_t timestampMs = 0;
    std::string type;
    std::string ticker;
    double quantity = 0.0;
    double pricePerUnit = 0.0;
    double fees = 0.0;
    std::string currency;
};

struct PortfolioSummary {
    std::string id;
    std::string name;
    std::string baseCurrency;
    double totalValue = 0.0;
    std::vector<CashRow> cashBalances;
    std::vector<PositionRow> positions;
};

struct PortfolioListEntry {
    std::string id;
    std::string name;
};

struct TimeSeriesData {
    std::vector<int64_t> timestamps;
    std::vector<double> values;
};

struct CreatePortfolioParams {
    std::string name;
    std::string currency;
};

struct AddTransactionParams {
    int64_t timestampMs = 0;
    std::string type;
    std::string ticker;
    double quantity = 0.0;
    double pricePerUnit = 0.0;
    double fees = 0.0;
    std::string currency;
};

}  // namespace finui
