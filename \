// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace finui {

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
    int64_t timestampMs = 0;
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

class IPortfolioDataSource {
 public:
    virtual ~IPortfolioDataSource() = default;

    virtual std::vector<PortfolioListEntry> listPortfolios() = 0;
    virtual PortfolioSummary loadSummary(const std::string& portfolioId) = 0;
    virtual std::vector<TransactionRow> listTransactions(const std::string& portfolioId) = 0;
    virtual std::string createPortfolio(const CreatePortfolioParams& p) = 0;
    virtual void deletePortfolio(const std::string& portfolioId) = 0;
    virtual std::string addTransaction(const std::string& portfolioId, const AddTransactionParams& p) = 0;
    virtual void deleteTransaction(const std::string& portfolioId, const std::string& txnId) = 0;
    virtual TimeSeriesData getTimeSeries(const std::string& portfolioId, int64_t startMs, int64_t endMs,
                                         int64_t deltaMs) = 0;
};

}  // namespace finui
