// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "finapp/finance/portfolio/PortfolioSnapshot.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"

namespace finapp {

class IPortfolioRepository {
 public:
    IPortfolioRepository() = default;
    virtual ~IPortfolioRepository() = default;
    IPortfolioRepository(const IPortfolioRepository&) = default;
    IPortfolioRepository& operator=(const IPortfolioRepository&) = default;
    IPortfolioRepository(IPortfolioRepository&&) = default;
    IPortfolioRepository& operator=(IPortfolioRepository&&) = default;

    virtual void saveSnapshot(const finance::PortfolioSnapshot& snapshot) = 0;
    virtual std::optional<finance::PortfolioSnapshot> loadLatestSnapshot(const std::string& portfolioId) const = 0;

    virtual void appendTransactions(const std::string& portfolioId, const std::vector<finance::Transaction>& transactions) = 0;
    virtual std::vector<finance::Transaction> loadTransactions(const std::string& portfolioId,
                                                      int64_t afterTimestamps = 0) const = 0;
    // Removes the transaction with the given id and invalidates all snapshots at or after its
    // timestampsMs, since those snapshots were built with the deleted transaction applied.
    virtual void deleteTransaction(const std::string& portfolioId, const std::string& transactionId) = 0;

    virtual std::vector<std::string> listPortfolioIds() const = 0;
    virtual bool exists(const std::string& portfolioId) const = 0;

    virtual void deletePortfolio(const std::string& portfolioId) = 0;  // Soft Delete
};

}  // namespace finapp
