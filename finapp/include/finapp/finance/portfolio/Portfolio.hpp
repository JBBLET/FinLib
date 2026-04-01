// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finapp/finance/asset/IAsset.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finapp/finance/portfolio/PortfolioSnapshot.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"

namespace finance {

struct TargetAllocation {
    std::string assetTicker;
    double weight;
};

class Portfolio {
 public:
    class Builder;

    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }
    Currency baseCurrency() const { return baseCurrency_; }

    const std::vector<Position>& positions() const { return positions_; }
    double cashBalance(Currency currency) const;

    const std::vector<TargetAllocation>& TargetAllocations() const;
    void setTargetAllocation(std::vector<TargetAllocation> targetAllocations) {
        TargetAllocations_ = std::move(targetAllocations);
    }
    void apply(const Transaction& transaction);

    std::vector<Transaction> rebalance(int64_t timestampsMs, const std::unordered_map<std::string, double>& prices,
                                       const std::unordered_map<std::string, double>& fxRates = {}) const;

    double totalValue(const std::unordered_map<std::string, double>& prices,
                      const std::unordered_map<std::string, double>& fxRates = {}) const;

    std::unordered_map<std::string, double> weights(const std::unordered_map<std::string, double>& prices,
                                                    const std::unordered_map<std::string, double>& fxRates = {}) const;

    PortfolioSnapshot snapshot(int64_t timestampsMs) const;
    void restoreFromSnapshot(const PortfolioSnapshot& snapshot);

 private:
    Portfolio(std::string id, std::string name, Currency baseCurrency);

    std::string id_;
    std::string name_;
    Currency baseCurrency_;

    std::vector<Position> positions_;
    std::unordered_map<Currency, double> cashBalances_;
    std::vector<TargetAllocation> TargetAllocations_;
    int64_t lastTransactionsMs_ = 0;

    std::unordered_map<std::string, size_t> positionsIndex_;

    void applyBuy_(const Transaction& Transaction);
    void applySell_(const Transaction& Transaction);
    void applyDeposit_(const Transaction& Transaction);
    void applyWithdrawal_(const Transaction& Transaction);
    void applyDividend_(const Transaction& Transaction);
    void applySplit_(const Transaction& Transaction);
};

class Portfolio::Builder {
 public:
    Builder(std::string id, std::string name, Currency baseCurrency);

    Builder& addPosition(std::shared_ptr<const IAsset> asset, double quantity);
    Builder& addCash(Currency currency, double amount);

    Builder& setTargetAllocation(std::vector<TargetAllocation> allocations);
    Builder& withInitialCapital(Currency currency, double amount);

    Builder& fromSnapshot(const PortfolioSnapshot& snapshot);
    Builder& withTransactions(std::vector<Transaction> transactions);

    Portfolio build();

 private:
    std::string id_;
    std::string name_;
    Currency baseCurrency_;
    std::vector<Position> positions_;
    std::unordered_map<Currency, double> cashBalances_;
    std::vector<TargetAllocation> targetAllocations_;
    std::vector<Transaction> transactions_;
    std::optional<PortfolioSnapshot> snapshot_;
};
}  // namespace finance
