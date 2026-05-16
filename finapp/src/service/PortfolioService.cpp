// Copyright (c) 2026 JBBLET. All Rights Reserved.

#include "finapp/service/PortfolioService.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finapp/finance/common/AssetId.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finapp/finance/portfolio/PortfolioSnapshot.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"
#include "finlib/common/utils/TimeSeriesUtils.hpp"
#include "finlib/core/TimeSeries.hpp"

namespace finapp {
using finance::AssetId;
using finance::AssetType;
using finance::Currency;
using finance::IAsset;
using finance::Portfolio;
using finance::PortfolioSnapshot;
using finance::SnapshotPosition;
using finance::TargetAllocation;
using finance::Transaction;
using finance::TransactionType;
// Daily — used for single-point market-data lookups (totalValue / weights / rebalance).
// The shared-timestamp path in TimeSeriesService::get requires >= 2 points to infer
// frequency, so singleton queries go through the (start, end, freq) overload instead.
namespace {
constexpr int64_t kDefaultSpotFrequencyMs = 86'400'000;

std::string cashKey(Currency c) { return "CASH:" + toString(c); }

std::string generateTransactionId() {
    static std::mt19937_64 gen{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dis;
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << dis(gen);
    return oss.str();
}
}  // namespace

PortfolioService::PortfolioService(std::shared_ptr<IPortfolioRepository> portfolioRepository,
                                   std::shared_ptr<AssetService> assetService, std::shared_ptr<FXService> fxService)
    : portfolioRepository_(std::move(portfolioRepository)),
      assetService_(std::move(assetService)),
      fxService_(std::move(fxService)) {}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

Portfolio PortfolioService::createNew(const std::string& portfolioId, const std::string& name, Currency baseCurrency,
                                      int64_t timestampMs) {
    if (portfolioRepository_->exists(portfolioId)) {
        throw std::runtime_error("PortfolioService::createNew: portfolio '" + portfolioId + "' already exists.");
    }
    Portfolio portfolio = Portfolio::Builder(portfolioId, name, baseCurrency).build();
    portfolioRepository_->saveSnapshot(portfolio.snapshot(timestampMs));
    return portfolio;
}

void PortfolioService::deletePortfolio(const std::string& portfolioId) {
    if (!portfolioRepository_->exists(portfolioId)) {
        throw std::runtime_error("PortfolioService::deletePortfolio: portfolio '" + portfolioId + "' does not exist.");
    }
    portfolioRepository_->deletePortfolio(portfolioId);
}

Portfolio PortfolioService::load(const std::string& portfolioId) {
    auto snapshotOpt = portfolioRepository_->loadLatestSnapshot(portfolioId);
    if (!snapshotOpt.has_value()) {
        throw std::runtime_error("PortfolioService::load: no snapshot found for portfolio " + portfolioId);
    }
    const PortfolioSnapshot& snapshot = *snapshotOpt;

    // Load transactions strictly after the snapshot — the snapshot already reflects any
    // transaction at its own timestamp.
    std::vector<Transaction> postTransactions;
    try {
        postTransactions = portfolioRepository_->loadTransactions(portfolioId, snapshot.timestampMs + 1);
    } catch (const std::runtime_error&) {
        // Transactions file may not exist yet — empty history is legal.
        postTransactions = {};
    }

    return Portfolio::Builder(portfolioId, snapshot.name, snapshot.baseCurrency)
        .fromSnapshot(snapshot)
        .withTransactions(std::move(postTransactions))
        .build();
}

void PortfolioService::save(const Portfolio& portfolio, int64_t timestampMs) {
    portfolioRepository_->saveSnapshot(portfolio.snapshot(timestampMs));
}

double PortfolioService::totalValue(const Portfolio& portfolio, int64_t timestampMs) {
    const Currency base = portfolio.baseCurrency();
    double total = 0.0;

    for (const SnapshotPosition& pos : portfolio.positions()) {
        auto asset = assetService_->load(pos.assetId);
        const Currency denom = asset->denomination();

        TimeSeries priceSeries =
            assetService_->loadTimeSeriesValue(pos.assetId, timestampMs, timestampMs, kDefaultSpotFrequencyMs);
        if (priceSeries.size() == 0) {
            throw std::runtime_error("PortfolioService::totalValue: no price available for " + pos.assetId.ticker);
        }
        const double price = priceSeries.getValues().front();

        double fx = 1.0;
        if (denom != base) {
            TimeSeries fxSeries = fxService_->load(denom, base, timestampMs, timestampMs, kDefaultSpotFrequencyMs);
            fx = fxSeries.getValues().front();
        }
        total += pos.quantity * price * fx;
    }

    for (const auto& [currency, amount] : portfolio.cashBalances()) {
        if (currency == base) {
            total += amount;
            continue;
        }
        TimeSeries fxSeries = fxService_->load(currency, base, timestampMs, timestampMs, kDefaultSpotFrequencyMs);
        total += amount * fxSeries.getValues().front();
    }

    return total;
}

std::unordered_map<std::string, double> PortfolioService::weights(const Portfolio& portfolio, int64_t timestampMs) {
    const Currency base = portfolio.baseCurrency();
    std::unordered_map<std::string, double> valueByKey;
    double total = 0.0;

    for (const SnapshotPosition& pos : portfolio.positions()) {
        auto asset = assetService_->load(pos.assetId);
        const Currency denom = asset->denomination();

        TimeSeries priceSeries =
            assetService_->loadTimeSeriesValue(pos.assetId, timestampMs, timestampMs, kDefaultSpotFrequencyMs);
        const double price = priceSeries.getValues().front();
        double fx = 1.0;
        if (denom != base) {
            TimeSeries fxSeries = fxService_->load(denom, base, timestampMs, timestampMs, kDefaultSpotFrequencyMs);
            fx = fxSeries.getValues().front();
        }
        const double value = pos.quantity * price * fx;
        valueByKey[pos.assetId.ticker] = value;
        total += value;
    }

    for (const auto& [currency, amount] : portfolio.cashBalances()) {
        double fx = 1.0;
        if (currency != base) {
            TimeSeries fxSeries = fxService_->load(currency, base, timestampMs, timestampMs, kDefaultSpotFrequencyMs);
            fx = fxSeries.getValues().front();
        }
        const double value = amount * fx;
        valueByKey[cashKey(currency)] = value;
        total += value;
    }

    std::unordered_map<std::string, double> result;
    result.reserve(valueByKey.size());
    if (total <= 0.0) {
        // Degenerate portfolio — return zero weights rather than NaN.
        for (const auto& [key, _] : valueByKey) result[key] = 0.0;
        return result;
    }
    for (const auto& [key, value] : valueByKey) {
        result[key] = value / total;
    }
    return result;
}

std::vector<Transaction> PortfolioService::rebalance(const Portfolio& portfolio, int64_t timestampMs) {
    const auto& targets = portfolio.targetAllocations();
    if (targets.empty()) {
        return {};
    }
    const Currency base = portfolio.baseCurrency();
    const double total = totalValue(portfolio, timestampMs);

    // Index current positions by ticker for O(1) delta lookup.
    std::unordered_map<std::string, const SnapshotPosition*> currentByTicker;
    currentByTicker.reserve(portfolio.positions().size());
    for (const SnapshotPosition& pos : portfolio.positions()) {
        currentByTicker[pos.assetId.ticker] = &pos;
    }

    std::vector<Transaction> transactions;
    transactions.reserve(targets.size());

    for (const TargetAllocation& target : targets) {
        auto asset = assetService_->load(target.assetId);
        const Currency denom = asset->denomination();

        TimeSeries priceSeries =
            assetService_->loadTimeSeriesValue(target.assetId, timestampMs, timestampMs, kDefaultSpotFrequencyMs);
        const double price = priceSeries.getValues().front();
        double fx = 1.0;
        if (denom != base) {
            TimeSeries fxSeries = fxService_->load(denom, base, timestampMs, timestampMs, kDefaultSpotFrequencyMs);
            fx = fxSeries.getValues().front();
        }

        // Current value in base currency.
        double currentValueBase = 0.0;
        if (auto it = currentByTicker.find(target.assetId.ticker); it != currentByTicker.end()) {
            currentValueBase = it->second->quantity * price * fx;
        }
        const double targetValueBase = target.weight * total;
        const double deltaValueBase = targetValueBase - currentValueBase;

        // Convert delta back into asset-denomination quantity.
        if (price <= 0.0 || fx <= 0.0) {
            throw std::runtime_error("PortfolioService::rebalance: non-positive price/fx for " + target.assetId.ticker);
        }
        const double deltaQuantity = deltaValueBase / (price * fx);
        if (deltaQuantity == 0.0) continue;

        Transaction tx{};
        tx.timestampsMs = timestampMs;
        tx.type = deltaQuantity > 0.0 ? TransactionType::Buy : TransactionType::Sell;
        tx.assetType = target.assetId.type;
        tx.assetTicker = target.assetId.ticker;
        tx.quantity = std::abs(deltaQuantity);
        tx.pricePerUnit = price;
        tx.fees = 0.0;
        tx.settlementCurrency = denom;
        transactions.push_back(tx);
    }

    return transactions;
}

// ---------------------------------------------------------------------------
// Derived TimeSeries over a range
// ---------------------------------------------------------------------------

TimeSeries PortfolioService::valueSeries(const std::string& portfolioId, int64_t startMs, int64_t endMs,
                                         int64_t frequencyMs) {
    return valueSeries(portfolioId, common::utils::timeSeries::makeRegularTimestamps(startMs, endMs, frequencyMs));
}

std::unordered_map<std::string, TimeSeries> PortfolioService::weightSeries(const std::string& portfolioId,
                                                                           int64_t startMs, int64_t endMs,
                                                                           int64_t frequencyMs) {
    return weightSeries(portfolioId, common::utils::timeSeries::makeRegularTimestamps(startMs, endMs, frequencyMs));
}

TimeSeries PortfolioService::valueSeries(const std::string& portfolioId, TimestampPtr timestamps) {
    if (!timestamps || timestamps->empty()) {
        throw std::invalid_argument("PortfolioService::valueSeries: timestamps must be non-empty.");
    }

    // Rebuild the portfolio from snapshot + transactions that cover the requested window.
    auto snapshotOpt = portfolioRepository_->loadLatestSnapshot(portfolioId);
    if (!snapshotOpt.has_value()) {
        throw std::runtime_error("PortfolioService::valueSeries: no snapshot for portfolio " + portfolioId);
    }
    const PortfolioSnapshot& snapshot = *snapshotOpt;
    const Currency base = snapshot.baseCurrency;

    std::vector<Transaction> transactions;
    try {
        transactions = portfolioRepository_->loadTransactions(portfolioId, snapshot.timestampMs + 1);
    } catch (const std::runtime_error&) {
        transactions = {};
    }
    // Transactions must be chronological for the walk — the CSV path is already sorted by
    // timestamp but we can't assume that about every repo impl.
    std::sort(transactions.begin(), transactions.end(), [](const Transaction& a, const Transaction& b) {
        if (a.timestampsMs != b.timestampsMs) return a.timestampsMs < b.timestampsMs;
        // Deposits before Buys on the same timestamp — mirrors Portfolio::Builder sort.
        auto pri = [](TransactionType t) -> int {
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
        };
        return pri(a.type) < pri(b.type);
    });

    Portfolio runningPortfolio = Portfolio::Builder(portfolioId, snapshot.name, base).fromSnapshot(snapshot).build();

    // Collect the universe of assets and currencies that ever appear in snapshot + transactions.
    std::unordered_set<AssetId> uniqueAssetIds;
    std::unordered_set<Currency> uniqueCurrencies;
    for (const SnapshotPosition& p : snapshot.positions) {
        uniqueAssetIds.insert(p.assetId);
    }
    for (const auto& [currency, _] : snapshot.cashBalances) {
        uniqueCurrencies.insert(currency);
    }
    for (const Transaction& tx : transactions) {
        if (tx.assetType != AssetType::Cash) {
            uniqueAssetIds.insert(AssetId{tx.assetType, tx.assetTicker});
        }
        uniqueCurrencies.insert(tx.settlementCurrency);
    }

    // Prefetch asset descriptors + price series aligned on the caller's timestamp grid.
    struct AssetData {
        std::shared_ptr<const IAsset> asset;
        TimeSeries prices;
    };
    std::unordered_map<AssetId, AssetData> assetData;
    assetData.reserve(uniqueAssetIds.size());
    for (const AssetId& aid : uniqueAssetIds) {
        auto asset = assetService_->load(aid);
        uniqueCurrencies.insert(asset->denomination());
        TimeSeries prices = assetService_->loadTimeSeriesValue(aid, timestamps);
        assetData.emplace(aid, AssetData{std::move(asset), std::move(prices)});
    }

    // Prefetch FX series for every non-base currency in play.
    std::unordered_map<Currency, TimeSeries> fxSeries;
    fxSeries.reserve(uniqueCurrencies.size());
    for (Currency c : uniqueCurrencies) {
        if (c == base) continue;
        fxSeries.emplace(c, fxService_->load(c, base, timestamps));
    }

    const auto& ts = *timestamps;
    std::vector<double> values(ts.size(), 0.0);
    size_t txIndex = 0;

    for (size_t i = 0; i < ts.size(); ++i) {
        const int64_t tick = ts[i];
        // Apply any pending transactions up to and including this tick before we evaluate.
        while (txIndex < transactions.size() && transactions[txIndex].timestampsMs <= tick) {
            runningPortfolio.apply(transactions[txIndex]);
            ++txIndex;
        }

        double total = 0.0;
        for (const SnapshotPosition& pos : runningPortfolio.positions()) {
            auto dataIt = assetData.find(pos.assetId);
            if (dataIt == assetData.end()) continue;  // defensive — should never happen
            const double price = dataIt->second.prices.getValues()[i];
            const Currency denom = dataIt->second.asset->denomination();
            double fx = 1.0;
            if (denom != base) {
                fx = fxSeries.at(denom).getValues()[i];
            }
            total += pos.quantity * price * fx;
        }
        for (const auto& [currency, amount] : runningPortfolio.cashBalances()) {
            double fx = 1.0;
            if (currency != base) {
                fx = fxSeries.at(currency).getValues()[i];
            }
            total += amount * fx;
        }
        values[i] = total;
    }

    return TimeSeries(portfolioId + "_value", std::move(timestamps), std::move(values));
}

std::unordered_map<std::string, TimeSeries> PortfolioService::weightSeries(const std::string& portfolioId,
                                                                           TimestampPtr timestamps) {
    if (!timestamps || timestamps->empty()) {
        throw std::invalid_argument("PortfolioService::weightSeries: timestamps must be non-empty.");
    }

    auto snapshotOpt = portfolioRepository_->loadLatestSnapshot(portfolioId);
    if (!snapshotOpt.has_value()) {
        throw std::runtime_error("PortfolioService::weightSeries: no snapshot for portfolio " + portfolioId);
    }
    const PortfolioSnapshot& snapshot = *snapshotOpt;
    const Currency base = snapshot.baseCurrency;

    std::vector<Transaction> transactions;
    try {
        transactions = portfolioRepository_->loadTransactions(portfolioId, snapshot.timestampMs + 1);
    } catch (const std::runtime_error&) {
        transactions = {};
    }
    std::sort(transactions.begin(), transactions.end(), [](const Transaction& a, const Transaction& b) {
        if (a.timestampsMs != b.timestampsMs) return a.timestampsMs < b.timestampsMs;
        // Deposits before Buys on the same timestamp — mirrors Portfolio::Builder sort.
        auto pri = [](TransactionType t) -> int {
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
        };
        return pri(a.type) < pri(b.type);
    });

    Portfolio runningPortfolio = Portfolio::Builder(portfolioId, snapshot.name, base).fromSnapshot(snapshot).build();

    std::unordered_set<AssetId> uniqueAssetIds;
    std::unordered_set<Currency> uniqueCurrencies;
    for (const SnapshotPosition& p : snapshot.positions) uniqueAssetIds.insert(p.assetId);
    for (const auto& [c, _] : snapshot.cashBalances) uniqueCurrencies.insert(c);
    for (const Transaction& tx : transactions) {
        if (tx.assetType != AssetType::Cash) {
            uniqueAssetIds.insert(AssetId{tx.assetType, tx.assetTicker});
        }
        uniqueCurrencies.insert(tx.settlementCurrency);
    }

    struct AssetData {
        std::shared_ptr<const IAsset> asset;
        TimeSeries prices;
    };
    std::unordered_map<AssetId, AssetData> assetData;
    assetData.reserve(uniqueAssetIds.size());
    for (const AssetId& aid : uniqueAssetIds) {
        auto asset = assetService_->load(aid);
        uniqueCurrencies.insert(asset->denomination());
        TimeSeries prices = assetService_->loadTimeSeriesValue(aid, timestamps);
        assetData.emplace(aid, AssetData{std::move(asset), std::move(prices)});
    }

    std::unordered_map<Currency, TimeSeries> fxSeries;
    fxSeries.reserve(uniqueCurrencies.size());
    for (Currency c : uniqueCurrencies) {
        if (c == base) continue;
        fxSeries.emplace(c, fxService_->load(c, base, timestamps));
    }

    // Accumulate per-key value vectors alongside the running total, then normalize at the end.
    const auto& ts = *timestamps;
    std::unordered_map<std::string, std::vector<double>> valuesByKey;
    std::vector<double> totals(ts.size(), 0.0);
    size_t txIndex = 0;

    auto ensureKey = [&](const std::string& key) -> std::vector<double>& {
        auto [it, inserted] = valuesByKey.try_emplace(key, ts.size(), 0.0);
        return it->second;
    };

    for (size_t i = 0; i < ts.size(); ++i) {
        const int64_t tick = ts[i];
        while (txIndex < transactions.size() && transactions[txIndex].timestampsMs <= tick) {
            runningPortfolio.apply(transactions[txIndex]);
            ++txIndex;
        }

        double total = 0.0;
        for (const SnapshotPosition& pos : runningPortfolio.positions()) {
            auto dataIt = assetData.find(pos.assetId);
            if (dataIt == assetData.end()) continue;
            const double price = dataIt->second.prices.getValues()[i];
            const Currency denom = dataIt->second.asset->denomination();
            double fx = 1.0;
            if (denom != base) fx = fxSeries.at(denom).getValues()[i];
            const double value = pos.quantity * price * fx;
            ensureKey(pos.assetId.ticker)[i] = value;
            total += value;
        }
        for (const auto& [currency, amount] : runningPortfolio.cashBalances()) {
            double fx = 1.0;
            if (currency != base) fx = fxSeries.at(currency).getValues()[i];
            const double value = amount * fx;
            ensureKey(cashKey(currency))[i] = value;
            total += value;
        }
        totals[i] = total;
    }

    // Normalize into weight series, sharing the caller's timestamp pointer.
    std::unordered_map<std::string, TimeSeries> result;
    result.reserve(valuesByKey.size());
    for (auto& [key, valueVec] : valuesByKey) {
        for (size_t i = 0; i < valueVec.size(); ++i) {
            valueVec[i] = totals[i] > 0.0 ? valueVec[i] / totals[i] : 0.0;
        }
        result.emplace(key, TimeSeries(portfolioId + "_weight_" + key, timestamps, std::move(valueVec)));
    }
    return result;
}

// ---------------------------------------------------------------------------
// Listing and transaction management
// ---------------------------------------------------------------------------

std::vector<std::string> PortfolioService::listPortfolioIds() { return portfolioRepository_->listPortfolioIds(); }

std::vector<Transaction> PortfolioService::listTransactions(const std::string& portfolioId, int64_t afterTimestampMs) {
    return portfolioRepository_->loadTransactions(portfolioId, afterTimestampMs);
}

std::string PortfolioService::addTransaction(const std::string& portfolioId, Transaction transaction,
                                             int64_t timestampMs) {
    transaction.id = generateTransactionId();
    Portfolio portfolio = load(portfolioId);
    portfolio.apply(transaction);
    portfolioRepository_->appendTransactions(portfolioId, {transaction});
    save(portfolio, timestampMs);
    return transaction.id;
}

void PortfolioService::deleteTransaction(const std::string& portfolioId, const std::string& transactionId) {
    portfolioRepository_->deleteTransaction(portfolioId, transactionId);
}

std::string PortfolioService::updateTransaction(const std::string& portfolioId, const std::string& transactionId,
                                                Transaction corrected, int64_t timestampMs) {
    deleteTransaction(portfolioId, transactionId);
    return addTransaction(portfolioId, std::move(corrected), timestampMs);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void PortfolioService::recomputeAndCache_(const Portfolio&, int64_t, int64_t, int64_t) {
    // Intentionally empty — valueSeries/weightSeries already rely on TimeSeriesService
    // for caching the underlying market-data series, and re-walking the portfolio is
    // cheap compared to the fetch cost. Kept as a hook for future memoization.
}

}  // namespace finapp
