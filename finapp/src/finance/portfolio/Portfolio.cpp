// Copyright (c) 2026 JBBLET. All Rights Reserved.

#include "finapp/finance/portfolio/Portfolio.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "finapp/common/Error.hpp"
#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/common/AssetId.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finapp/finance/portfolio/PortfolioSnapshot.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"
#include "finlib/common/utils/TimeSeriesUtils.hpp"
#include "finlib/core/TimeSeries.hpp"

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
        newPositionIndex[i->assetId.ticker] = index;
        index++;
    }
    return newPositionIndex;
}

void Portfolio::Builder::checkPositions_() {
    std::unordered_set<AssetId> assetIdSeen;
    for (const auto& snapshotPosition : positions_) {
        ensure(assetIdSeen.insert(snapshotPosition.assetId).second, "Two or more Positions of the same Asset");
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
    std::sort(transactions_.begin(), transactions_.end(), [](const Transaction& a, const Transaction& b) {
        if (a.timestampsMs != b.timestampsMs) return a.timestampsMs < b.timestampsMs;
        return transactionTypePriority(a.type) < transactionTypePriority(b.type);
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
    cashBalances_[currency] += amount;
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
    name_ = snapshot.name;
    baseCurrency_ = snapshot.baseCurrency;
    positions_ = snapshot.positions;
    cashBalances_ = snapshot.cashBalances;
    lastTransactionMs_ = snapshot.timestampMs;
    positionsIndex_ = std::move(getPositionsIndexFromPosition(positions_));
}

PortfolioSnapshot Portfolio::snapshot(Timestamp timestampsMs) const {
    return PortfolioSnapshot{name_, baseCurrency_, timestampsMs, id_, positions_, cashBalances_};
}

// Computations
PortfolioSeries Portfolio::valueAndWeightSeries(
    ts::TimestampsPtr grid, const std::unordered_map<finance::AssetId, ts::TimeSeries>& priceInBase,
    const std::unordered_map<finance::Currency, ts::TimeSeries>& fxToBase) const {
    ts::TimeSeries total = ts::common::utils::timeSeries::generateConstantTimeSeries(id_ + "_value", grid, 0.0);
    std::unordered_map<finance::AssetId, ts::TimeSeries> assetValues = {};
    assetValues.reserve(positions_.size() + cashBalances_.size());
    for (const auto& pos : positions_) {
        if (pos.quantity == 0.0) continue;
        auto pv = priceInBase.at(pos.assetId) * pos.quantity;
        assetValues[pos.assetId] = pv;
        total += pv;
    }
    for (const auto& [currency, amount] : cashBalances_) {
        auto cashVal = fxToBase.at(currency) * amount;
        total += cashVal;
        assetValues[AssetId{AssetType::Cash, toString(currency)}] = cashVal;
    }
    std::unordered_map<finance::AssetId, ts::TimeSeries> weights{};
    weights.reserve(assetValues.size());
    for (const auto& [assetId, timeSeries] : assetValues) {
        weights[assetId] = timeSeries / total;
    }
    return {total, weights};
}

ts::TimeSeries Portfolio::valueSeries(ts::TimestampsPtr grid,
                                      const std::unordered_map<finance::AssetId, ts::TimeSeries>& priceInBase,
                                      const std::unordered_map<finance::Currency, ts::TimeSeries>& fxToBase) const {
    ts::TimeSeries total = ts::common::utils::timeSeries::generateConstantTimeSeries(id_ + "_value", grid, 0.0);
    for (const auto& pos : positions_) {
        if (pos.quantity == 0.0) continue;
        auto pv = priceInBase.at(pos.assetId) * pos.quantity;
        total += pv;
    }
    for (const auto& [currency, amount] : cashBalances_) {
        auto cashVal = fxToBase.at(currency) * amount;
        total += cashVal;
    }
    return total;
}

PortfolioValuation Portfolio::valuation(const std::unordered_map<finance::AssetId, double>& priceInBase,
                                        const std::unordered_map<finance::Currency, double>& fxToBase) const {
    double total = 0.0;
    std::unordered_map<finance::AssetId, double> assetValues{};
    assetValues.reserve(positions_.size() + cashBalances_.size());
    for (const auto& pos : positions_) {
        if (pos.quantity == 0.0) continue;
        const double pv = priceInBase.at(pos.assetId) * pos.quantity;
        assetValues[pos.assetId] = pv;
        total += pv;
    }
    for (const auto& [currency, amount] : cashBalances_) {
        const double cashVal = fxToBase.at(currency) * amount;
        assetValues[AssetId{AssetType::Cash, toString(currency)}] = cashVal;
        total += cashVal;
    }
    std::unordered_map<finance::AssetId, double> weights{};
    weights.reserve(assetValues.size());
    // Explicit guard: the series twin gets total==0 -> 0 for free from operator/, but scalar
    // division has no such protection.
    for (const auto& [assetId, value] : assetValues) weights[assetId] = (total == 0.0) ? 0.0 : value / total;
    return {total, weights};
}

void Portfolio::apply(const Transaction& transaction) {
    ensure(transaction.timestampsMs >= lastTransactionMs_,
           "The Transaction is outdated relative to the Portfolio ({} < {})", transaction.timestampsMs,
           lastTransactionMs_);
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
            throw finapp::Exception("Illegal TransactionType");
    }
}

void Portfolio::applyBuy_(const Transaction& transaction) {
    ensure(transaction.type == TransactionType::Buy, "The Transaction is not a Buy transaction");
    if (transaction.usedCurrency.has_value()) {
        cashBalances_[transaction.usedCurrency.value()] -= transaction.paymentprice.value();
    } else {
        double totalCost = transaction.quantity * transaction.pricePerUnit + transaction.fees;
        cashBalances_[transaction.settlementCurrency] -= totalCost;
    }
    try {
        size_t positionIndex = positionsIndex_.at(transaction.assetTicker);
        positions_[positionIndex].quantity += transaction.quantity;
    } catch (const std::out_of_range& e) {
        positions_.push_back(
            SnapshotPosition{AssetId{transaction.assetType, transaction.assetTicker}, transaction.quantity});
        positionsIndex_[transaction.assetTicker] = positions_.size() - 1;
    }
    lastTransactionMs_ = transaction.timestampsMs;
}

void Portfolio::applySell_(const Transaction& transaction) {
    ensure(transaction.type == TransactionType::Sell, "The Transaction is not a Sell Transaction");
    try {
        size_t positionIndex = positionsIndex_.at(transaction.assetTicker);
        double quantity = positions_[positionIndex].quantity;
        ensure(quantity >= transaction.quantity, "Not enough quantity to fulfill the transaction ({} < {})", quantity,
               transaction.quantity);
        positions_[positionIndex].quantity -= transaction.quantity;
    } catch (const std::out_of_range& e) {
        throw finapp::Exception("Not enough quantity to fulfill the transaction");
    }
    double totalRevenue = transaction.quantity * transaction.pricePerUnit - transaction.fees;
    cashBalances_[transaction.settlementCurrency] += totalRevenue;
    lastTransactionMs_ = transaction.timestampsMs;
}

void Portfolio::applyDeposit_(const Transaction& transaction) {
    ensure(transaction.type == TransactionType::Deposit, "The Transaction is not a Deposit Transaction");
    ensure(transaction.quantity >= 0, "Cannot deposit negative amount: {}", transaction.quantity);
    cashBalances_[transaction.settlementCurrency] += transaction.quantity - transaction.fees;
    lastTransactionMs_ = transaction.timestampsMs;
}

void Portfolio::applyWithdrawal_(const Transaction& transaction) {
    ensure(transaction.type == TransactionType::Withdrawal, "The Transaction is not a Withdrawal transaction");
    ensure(transaction.quantity >= 0, "Cannot withdraw a negative amount: {}", transaction.quantity);
    cashBalances_[transaction.settlementCurrency] -= transaction.quantity + transaction.fees;
    lastTransactionMs_ = transaction.timestampsMs;
}

void Portfolio::applyDividend_(const Transaction& transaction) {
    ensure(transaction.type == TransactionType::Dividend, "The Transaction is not a Dividend transaction");
    ensure(positionsIndex_.contains(transaction.assetTicker),
           "Cannot apply a Dividend if you do not hold the position '{}'", transaction.assetTicker);
    double sharesNumber = positions_[positionsIndex_[transaction.assetTicker]].quantity;
    if (sharesNumber == 0.0) {
        lastTransactionMs_ = transaction.timestampsMs;
        return;
    }
    cashBalances_[transaction.settlementCurrency] += sharesNumber * transaction.pricePerUnit - transaction.fees;
    lastTransactionMs_ = transaction.timestampsMs;
}

void Portfolio::applySplit_(const Transaction& transaction) {
    ensure(transaction.type == TransactionType::Split, "The Transaction is not a Split transaction");
    double sharesNumber = 0.0;
    try {
        size_t positionIndex = positionsIndex_.at(transaction.assetTicker);
        sharesNumber = positions_[positionIndex].quantity;
        positions_[positionIndex].quantity = transaction.quantity * sharesNumber;
    } catch (const std::out_of_range& e) {
        throw finapp::Exception("No Shares corresponding to this transaction in the Portfolio");
    }
    lastTransactionMs_ = transaction.timestampsMs;
}
}  // namespace finance
