// Copyright (c) 2026 JBBLET. All Rights Reserved.

#include "finapp/finance/portfolio/Portfolio.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "finapp/finance/common/Currency.hpp"
#include "finapp/finance/portfolio/PortfolioSnapshot.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"

namespace finance {
// TODO(JBBLET) add extra transaction check such as Dividend on a bond does not make sense
// add extra Transaction coupon payment for a bond or even FX Transaction
// allow short selling of assets
// Add AssetId to the Transaction rather than the Transaction assetTicker
// more assets such as options or futures

// Builder Implementation
// Helper functions
std::unordered_map<std::string, size_t> getPositionsIndexFromPosition(const std::vector<SnapshotPosition>& positions) {
    size_t index = 0;
    std::unordered_map<std::string, size_t> newPositionIndex;
    for (auto i = positions.begin(); i != positions.end(); i++) {
        newPositionIndex.at(i->assetId.ticker) = index;
        index++;
    }
    return newPositionIndex;
}

void Portfolio::Builder::checkPositions_() {
    std::unordered_set<AssetId> assetIdSeen;
    for (const auto& snapshotPosition : positions_) {
        if (!assetIdSeen.insert(snapshotPosition.assetId).second) {
            throw std::runtime_error("Two or more Positions of the same Asset");
        }
    }
}

Portfolio Portfolio::Builder::build() {
    Portfolio constructedPortfolio(id_, name_, baseCurrency_);
    if (snapshot_.has_value()) {
        constructedPortfolio.restoreFromSnapshot(snapshot_.value());
    } else {
        constructedPortfolio.positions_ = positions_;
        constructedPortfolio.cashBalances_ = cashBalances_;
        checkPositions_();
        constructedPortfolio.positionsIndex_ = std::move(getPositionsIndexFromPosition(positions_));
    }
    constructedPortfolio.universe_ = universe_;
    constructedPortfolio.targetAllocations_ = targetAllocations_;
    std::sort(transactions_.begin(), transactions_.end(),
              [](const Transaction& transactionA, const Transaction& transactionB) {
                  return transactionA.timestampsMs < transactionB.timestampsMs;
              });
    std::ranges::for_each(transactions_, [&constructedPortfolio](const Transaction& transaction) {
        constructedPortfolio.apply(transaction);
    });
    return constructedPortfolio;
}

Portfolio::Builder& Portfolio::Builder::addPosition(const AssetId& assetId, double quantity) {
    positions_.push_back({assetId, quantity});
    return *this;
}

Portfolio::Builder& Portfolio::Builder::addCash(Currency currency, double amount) {
    try {
        cashBalances_.at(currency) += amount;
    } catch (const std::out_of_range& e) {
        cashBalances_.at(currency) = amount;
    }
    return *this;
}

Portfolio::Builder& Portfolio::Builder::withInitialCapital(Currency currency, double amount) {
    addCash(currency, amount);
    return *this;
}

Portfolio::Builder& Portfolio::Builder::fromSnapshot(const PortfolioSnapshot& snapshot) {
    snapshot_ = snapshot;
    return *this;
}

Portfolio::Builder& Portfolio::Builder::withTransactions(std::vector<Transaction> transactions) {
    transactions_ = std::move(transactions);
    return *this;
}

Portfolio::Builder& Portfolio::Builder::setTargetAllocations(std::vector<TargetAllocation> allocations) {
    targetAllocations_ = std::move(allocations);
    return *this;
}

Portfolio::Builder& Portfolio::Builder::setUniverse(std::vector<AssetId> universe) {
    universe_ = std::move(universe);
    return *this;
}

void Portfolio::restoreFromSnapshot(const PortfolioSnapshot& snapshot) {
    positions_ = snapshot.positions;
    cashBalances_ = snapshot.cashBalances;
    lastTransactionMs_ = snapshot.timestampMs;
    positionsIndex_ = std::move(getPositionsIndexFromPosition(positions_));
}

PortfolioSnapshot Portfolio::snapshot(int64_t timestampsMs) const {
    return PortfolioSnapshot{timestampsMs, id_, positions_, cashBalances_};
}

void Portfolio::apply(const Transaction& transaction) {
    if (transaction.timestampsMs < lastTransactionMs_) {
        throw std::runtime_error("The Transaction is outdated relative to the Portfolio");
    }
    switch (transaction.type) {
        case (TransactionType::Buy): {
            return applyBuy_(transaction);
        }
        case (TransactionType::Sell): {
            return applySell_(transaction);
        }
        case (TransactionType::Deposit): {
            return applyDeposit_(transaction);
        }
        case (TransactionType::Withdrawal): {
            return applyWithdrawal_(transaction);
        }
        case (TransactionType::Dividend): {
            return applyDividend_(transaction);
        }
        case (TransactionType::Split): {
            return applySplit_(transaction);
        }
        default:
            throw std::runtime_error("Illegal TransactionType");
    }
}

void Portfolio::applyBuy_(const Transaction& transaction) {
    if (transaction.type != TransactionType::Buy) {
        throw std::runtime_error("The Transaction is not a Buy transaction");
    }
    double totalCost = transaction.quantity * transaction.pricePerUnit + transaction.fees;
    double remainingFund = 0.0;
    try {
        remainingFund = cashBalances_.at(transaction.SettlementCurrency);
    } catch (const std::out_of_range& e) {
        throw std::runtime_error("No funds in this currency to settle the transaction");
    }
    if (totalCost > remainingFund) {
        throw std::runtime_error("Insufficient funds to buy");
    }
    cashBalances_.at(transaction.SettlementCurrency) = remainingFund - totalCost;
    try {
        size_t positionIndex = positionsIndex_.at(transaction.assetTicker);
        positions_[positionIndex].quantity += transaction.quantity;
    } catch (const std::out_of_range& e) {
        positions_.push_back(
            SnapshotPosition{AssetId{transaction.assetType, transaction.assetTicker}, transaction.quantity});
        positionsIndex_.at(transaction.assetTicker) = positions_.size() - 1;
    }
    lastTransactionMs_ = transaction.timestampsMs;
}
void Portfolio::applySell_(const Transaction& transaction) {
    if (transaction.type != TransactionType::Sell) {
        throw std::runtime_error("The Transaction is not a Sell Transaction");
    }
    try {
        size_t positionIndex = positionsIndex_.at(transaction.assetTicker);
        double quantity = positions_[positionIndex].quantity;
        if (quantity < transaction.quantity) {
            throw std::runtime_error("Not enough quantity to fulfill the transaction");
        }
        positions_[positionIndex].quantity -= transaction.quantity;
    } catch (const std::out_of_range& e) {
        throw std::runtime_error("Not enough quantity to fulfill the transaction");
    }
    double totalRevenue = transaction.quantity * transaction.pricePerUnit - transaction.fees;
    try {
        cashBalances_.at(transaction.SettlementCurrency) += totalRevenue;
    } catch (const std::out_of_range& e) {
        cashBalances_.at(transaction.SettlementCurrency) = totalRevenue;
    }
    lastTransactionMs_ = transaction.timestampsMs;
}

void Portfolio::applyDeposit_(const Transaction& transaction) {
    if (transaction.type != TransactionType::Deposit) {
        throw std::runtime_error("The Transaction is not a Deposit Transaction");
    }
    if (transaction.quantity < 0) {
        throw std::runtime_error("Cannot deposit negative amount");
    }
    try {
        cashBalances_.at(transaction.SettlementCurrency) += transaction.quantity - transaction.fees;
    } catch (const std::out_of_range& e) {
        cashBalances_.at(transaction.SettlementCurrency) = transaction.quantity - transaction.fees;
    }
    lastTransactionMs_ = transaction.timestampsMs;
}
void Portfolio::applyWithdrawal_(const Transaction& transaction) {
    if (transaction.type != TransactionType::Withdrawal) {
        throw std::runtime_error("The Transaction is not a Withdrawal transaction");
    }
    if (transaction.quantity < 0) {
        throw std::runtime_error("Cannot withdraw a negative amount");
    }
    double currentAmmount = 0.0;
    try {
        currentAmmount = cashBalances_.at(transaction.SettlementCurrency);
        if (currentAmmount < transaction.quantity + transaction.fees) {
            throw std::runtime_error("Not enough currency to cover the Withdrawal");
        }
        cashBalances_.at(transaction.SettlementCurrency) -= transaction.quantity + transaction.fees;
    } catch (const std::out_of_range& e) {
        throw std::runtime_error("No currency to withdraw");
    }
    lastTransactionMs_ = transaction.timestampsMs;
}

void Portfolio::applyDividend_(const Transaction& transaction) {
    if (transaction.type != TransactionType::Dividend) {
        throw std::runtime_error("The Transaction is not a Dividend transaction");
    }
    double sharesNumber = 0.0;
    try {
        size_t positionIndex = positionsIndex_.at(transaction.assetTicker);
        sharesNumber = positions_[positionIndex].quantity;
    } catch (const std::out_of_range& e) {
    }
    try {
        cashBalances_.at(transaction.SettlementCurrency) += sharesNumber * transaction.pricePerUnit - transaction.fees;
    } catch (const std::out_of_range& e) {
        cashBalances_.at(transaction.SettlementCurrency) = sharesNumber * transaction.pricePerUnit - transaction.fees;
    }
    lastTransactionMs_ = transaction.timestampsMs;
}

void Portfolio::applySplit_(const Transaction& transaction) {
    if (transaction.type != TransactionType::Split) {
        throw std::runtime_error("The Transaction is not a Split transaction");
    }
    double sharesNumber = 0.0;
    try {
        size_t positionIndex = positionsIndex_.at(transaction.assetTicker);
        sharesNumber = positions_[positionIndex].quantity;
        positions_[positionIndex].quantity = transaction.quantity * sharesNumber;
    } catch (const std::out_of_range& e) {
        throw std::runtime_error("No Shares corresponding to this transaction in the Portfolio");
    }
    lastTransactionMs_ = transaction.timestampsMs;
}
}  // namespace finance
