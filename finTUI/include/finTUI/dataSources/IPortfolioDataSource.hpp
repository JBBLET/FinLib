// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "finTUI/modules/portfolioModule/PortfolioModuleTypes.hpp"

namespace finui {

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
    virtual void importCsv(const std::string& portfolioId, const std::string& csvData) = 0;
};

}  // namespace finui
