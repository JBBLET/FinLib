// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finapp/finance/common/AssetId.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finapp/finance/portfolio/PortfolioSnapshot.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"

namespace finance {

struct TargetAllocation {
    AssetId assetId;
    double weight;  // [0.0, 1.0], must sum to 1.0 across all allocations
};

class Portfolio {
 public:
    class Builder;

    // Identity
    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }
    Currency baseCurrency() const { return baseCurrency_; }

    // State
    const std::vector<SnapshotPosition>& positions() const { return positions_; }
    double cashBalance(Currency currency) const;

    // Investment universe — the set of assets this portfolio is allowed to hold
    const std::vector<AssetId>& universe() const { return universe_; }
    void setUniverse(std::vector<AssetId> universe) { universe_ = std::move(universe); }

    // Target allocations — must reference assets within the universe
    const std::vector<TargetAllocation>& targetAllocations() const { return targetAllocations_; }
    void setTargetAllocations(std::vector<TargetAllocation> targetAllocations) {
        targetAllocations_ = std::move(targetAllocations);
    }

    // Mutations
    void apply(const Transaction& transaction);

    // Persistence
    PortfolioSnapshot snapshot(int64_t timestampMs) const;
    void restoreFromSnapshot(const PortfolioSnapshot& snapshot);

 private:
    Portfolio(std::string id, std::string name, Currency baseCurrency);

    std::string id_;
    std::string name_;
    Currency baseCurrency_;

    std::vector<SnapshotPosition> positions_;
    std::unordered_map<Currency, double> cashBalances_;
    std::vector<AssetId> universe_;
    std::vector<TargetAllocation> targetAllocations_;
    int64_t lastTransactionMs_ = 0;

    std::unordered_map<std::string, size_t> positionsIndex_;

    void applyBuy_(const Transaction& transaction);
    void applySell_(const Transaction& transaction);
    void applyDeposit_(const Transaction& transaction);
    void applyWithdrawal_(const Transaction& transaction);
    void applyDividend_(const Transaction& transaction);
    void applySplit_(const Transaction& transaction);
};

class Portfolio::Builder {
 public:
    Builder(std::string id, std::string name, Currency baseCurrency);

    Builder& addPosition(const AssetId& assetId, double quantity);
    Builder& addCash(Currency currency, double amount);

    Builder& setUniverse(std::vector<AssetId> universe);
    Builder& setTargetAllocations(std::vector<TargetAllocation> allocations);
    Builder& withInitialCapital(Currency currency, double amount);

    Builder& fromSnapshot(const PortfolioSnapshot& snapshot);
    Builder& withTransactions(std::vector<Transaction> transactions);

    Portfolio build();

 private:
    std::string id_;
    std::string name_;
    Currency baseCurrency_;
    std::vector<SnapshotPosition> positions_;
    std::unordered_map<Currency, double> cashBalances_;
    std::vector<AssetId> universe_;
    std::vector<TargetAllocation> targetAllocations_;
    std::vector<Transaction> transactions_;
    std::optional<PortfolioSnapshot> snapshot_;
};
}  // namespace finance
