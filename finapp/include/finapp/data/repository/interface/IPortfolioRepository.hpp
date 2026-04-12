// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "finapp/finance/portfolio/PortfolioSnapshot.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"

namespace finance {

class IPortfolioRepository {
 public:
    virtual ~IPortfolioRepository() = default;

    virtual void saveSnapshot(const PortfolioSnapshot& snapshot) = 0;
    virtual std::optional<PortfolioSnapshot> loadLatestSnapshot(const std::string& portfolioId) const = 0;

    virtual void appendTransactions(const std::string& portfolioId, const std::vector<Transaction>& transactions) = 0;
    virtual std::vector<Transaction> loadTransactions(const std::string& portfolioId,
                                                      int64_t afterTimestamps = 0) const = 0;

    virtual std::vector<std::string> listPortfolioIds() const = 0;
    virtual bool exists(const std::string& portfolioId) const = 0;
};

}  // namespace finance
