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

#include "finapp/common/logger/PrefixedLogger.hpp"

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
using finance::Currency;
using finance::IAsset;
using finance::Portfolio;
using finance::PortfolioSnapshot;
using finance::SnapshotPosition;
using finance::TargetAllocation;
using finance::Transaction;
using finance::TransactionType;

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
                                   std::shared_ptr<AssetService> assetService, std::shared_ptr<FXService> fxService,
                                   finapp::logging::ILogger* logger)
    : portfolioRepository_(std::move(portfolioRepository)),
      assetService_(std::move(assetService)),
      fxService_(std::move(fxService)),
      logger_(finapp::logging::PrefixedLogger::wrap(logger, "PortfolioService")) {}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

Portfolio PortfolioService::createNew(const std::string& portfolioId, const std::string& name, Currency baseCurrency,
                                      int64_t timestampMs) {
    if (portfolioRepository_->exists(portfolioId)) {
        throw std::runtime_error("PortfolioService::createNew: portfolio '" + portfolioId + "' already exists.");
    }
    if (logger_)
        logger_->write(finapp::logging::Level::Info,
                       "createNew: id='" + portfolioId + "' name='" + name + "' base=" + toString(baseCurrency));
    Portfolio portfolio = Portfolio::Builder(portfolioId, name, baseCurrency).build();
    portfolioRepository_->saveSnapshot(portfolio.snapshot(timestampMs - 1));
    return portfolio;
}

void PortfolioService::deletePortfolio(const std::string& portfolioId) {
    if (!portfolioRepository_->exists(portfolioId)) {
        throw std::runtime_error("PortfolioService::deletePortfolio: portfolio '" + portfolioId + "' does not exist.");
    }
    if (logger_) logger_->write(finapp::logging::Level::Info, "deletePortfolio: id='" + portfolioId + "'");
    portfolioRepository_->deletePortfolio(portfolioId);
}

Portfolio PortfolioService::load(const std::string& portfolioId) {
    if (logger_) logger_->write(finapp::logging::Level::Debug, "load: id='" + portfolioId + "'");
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
        if (pos.quantity == 0.0) continue;

        auto asset = assetService_->load(pos.assetId);
        const Currency denom = asset->denomination();

        // getResampled (via AssetService) handles weekends/holidays via Nearest — no manual lookback needed.
        const double price = assetService_->loadTimeSeriesValue(pos.assetId, timestampMs, timestampMs,
                                                                kDefaultSpotFrequencyMs)
                                 .getValues()
                                 .back();

        double fx = 1.0;
        if (denom != base) {
            fx = fxService_->load(denom, base, timestampMs, timestampMs, kDefaultSpotFrequencyMs)
                     .getValues()
                     .back();
        }
        total += pos.quantity * price * fx;
    }

    for (const auto& [currency, amount] : portfolio.cashBalances()) {
        if (currency == base) {
            total += amount;
            continue;
        }
        const double fx =
            fxService_->load(currency, base, timestampMs, timestampMs, kDefaultSpotFrequencyMs).getValues().back();
        total += amount * fx;
    }

    return total;
}

std::unordered_map<std::string, double> PortfolioService::weights(const Portfolio& portfolio, int64_t timestampMs) {
    const Currency base = portfolio.baseCurrency();
    std::unordered_map<std::string, double> valueByKey;
    double total = 0.0;

    for (const SnapshotPosition& pos : portfolio.positions()) {
        if (pos.quantity == 0.0) continue;

        auto asset = assetService_->load(pos.assetId);
        const Currency denom = asset->denomination();

        const double price = assetService_->loadTimeSeriesValue(pos.assetId, timestampMs, timestampMs,
                                                                kDefaultSpotFrequencyMs)
                                 .getValues()
                                 .back();
        double fx = 1.0;
        if (denom != base) {
            fx = fxService_->load(denom, base, timestampMs, timestampMs, kDefaultSpotFrequencyMs)
                     .getValues()
                     .back();
        }
        const double value = pos.quantity * price * fx;
        valueByKey[pos.assetId.ticker] = value;
        total += value;
    }

    for (const auto& [currency, amount] : portfolio.cashBalances()) {
        double fx = 1.0;
        if (currency != base) {
            fx = fxService_->load(currency, base, timestampMs, timestampMs, kDefaultSpotFrequencyMs)
                     .getValues()
                     .back();
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

        const double price = assetService_->loadTimeSeriesValue(target.assetId, timestampMs, timestampMs,
                                                                kDefaultSpotFrequencyMs)
                                 .getValues()
                                 .back();
        double fx = 1.0;
        if (denom != base) {
            fx = fxService_->load(denom, base, timestampMs, timestampMs, kDefaultSpotFrequencyMs).getValues().back();
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

    auto allSnapshots = portfolioRepository_->loadAllSnapshots(portfolioId);
    if (allSnapshots.empty()) {
        throw std::runtime_error("PortfolioService::valueSeries: no snapshot for portfolio " + portfolioId);
    }
    std::sort(allSnapshots.begin(), allSnapshots.end(), [](const PortfolioSnapshot& a, const PortfolioSnapshot& b) {
        return a.timestampMs < b.timestampMs;
    });
    const Currency base = allSnapshots.front().baseCurrency;

    // Collect every asset and currency that appears in any snapshot.
    std::unordered_set<AssetId> uniqueAssetIds;
    std::unordered_set<Currency> uniqueCurrencies;
    for (const PortfolioSnapshot& snap : allSnapshots) {
        for (const SnapshotPosition& pos : snap.positions) uniqueAssetIds.insert(pos.assetId);
        for (const auto& [c, _] : snap.cashBalances) uniqueCurrencies.insert(c);
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
        assetData.emplace(aid, AssetData{std::move(asset), assetService_->loadTimeSeriesValue(aid, timestamps)});
    }

    std::unordered_map<Currency, TimeSeries> fxSeries;
    fxSeries.reserve(uniqueCurrencies.size());
    for (Currency c : uniqueCurrencies) {
        if (c != base) fxSeries.emplace(c, fxService_->load(c, base, timestamps));
    }

    // Snapshot timestamps for binary search.
    std::vector<int64_t> snapTs;
    snapTs.reserve(allSnapshots.size());
    for (const PortfolioSnapshot& snap : allSnapshots) snapTs.push_back(snap.timestampMs);

    const auto& ts = *timestamps;
    std::vector<double> values(ts.size(), 0.0);

    for (size_t i = 0; i < ts.size(); ++i) {
        const int64_t tick = ts[i];
        // Last snapshot with timestampMs <= tick.
        auto it = std::upper_bound(snapTs.begin(), snapTs.end(), tick);
        if (it == snapTs.begin()) continue;
        --it;
        const PortfolioSnapshot& snap = allSnapshots[static_cast<size_t>(it - snapTs.begin())];

        double total = 0.0;
        for (const SnapshotPosition& pos : snap.positions) {
            auto dataIt = assetData.find(pos.assetId);
            if (dataIt == assetData.end()) continue;
            const double price = dataIt->second.prices.getValues()[i];
            const Currency denom = dataIt->second.asset->denomination();
            double fx = (denom != base) ? fxSeries.at(denom).getValues()[i] : 1.0;
            total += pos.quantity * price * fx;
        }
        for (const auto& [currency, amount] : snap.cashBalances) {
            double fx = (currency != base) ? fxSeries.at(currency).getValues()[i] : 1.0;
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

    auto allSnapshots = portfolioRepository_->loadAllSnapshots(portfolioId);
    if (allSnapshots.empty()) {
        throw std::runtime_error("PortfolioService::weightSeries: no snapshot for portfolio " + portfolioId);
    }
    std::sort(allSnapshots.begin(), allSnapshots.end(), [](const PortfolioSnapshot& a, const PortfolioSnapshot& b) {
        return a.timestampMs < b.timestampMs;
    });
    const Currency base = allSnapshots.front().baseCurrency;

    std::unordered_set<AssetId> uniqueAssetIds;
    std::unordered_set<Currency> uniqueCurrencies;
    for (const PortfolioSnapshot& snap : allSnapshots) {
        for (const SnapshotPosition& pos : snap.positions) uniqueAssetIds.insert(pos.assetId);
        for (const auto& [c, _] : snap.cashBalances) uniqueCurrencies.insert(c);
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
        assetData.emplace(aid, AssetData{std::move(asset), assetService_->loadTimeSeriesValue(aid, timestamps)});
    }

    std::unordered_map<Currency, TimeSeries> fxSeries;
    fxSeries.reserve(uniqueCurrencies.size());
    for (Currency c : uniqueCurrencies) {
        if (c != base) fxSeries.emplace(c, fxService_->load(c, base, timestamps));
    }

    std::vector<int64_t> snapTs;
    snapTs.reserve(allSnapshots.size());
    for (const PortfolioSnapshot& snap : allSnapshots) snapTs.push_back(snap.timestampMs);

    const auto& ts = *timestamps;
    std::unordered_map<std::string, std::vector<double>> valuesByKey;
    std::vector<double> totals(ts.size(), 0.0);

    auto ensureKey = [&](const std::string& key) -> std::vector<double>& {
        auto [it, _] = valuesByKey.try_emplace(key, ts.size(), 0.0);
        return it->second;
    };

    for (size_t i = 0; i < ts.size(); ++i) {
        const int64_t tick = ts[i];
        auto it = std::upper_bound(snapTs.begin(), snapTs.end(), tick);
        if (it == snapTs.begin()) continue;
        --it;
        const PortfolioSnapshot& snap = allSnapshots[static_cast<size_t>(it - snapTs.begin())];

        double total = 0.0;
        for (const SnapshotPosition& pos : snap.positions) {
            auto dataIt = assetData.find(pos.assetId);
            if (dataIt == assetData.end()) continue;
            const double price = dataIt->second.prices.getValues()[i];
            const Currency denom = dataIt->second.asset->denomination();
            double fx = (denom != base) ? fxSeries.at(denom).getValues()[i] : 1.0;
            const double value = pos.quantity * price * fx;
            ensureKey(pos.assetId.ticker)[i] = value;
            total += value;
        }
        for (const auto& [currency, amount] : snap.cashBalances) {
            double fx = (currency != base) ? fxSeries.at(currency).getValues()[i] : 1.0;
            const double value = amount * fx;
            ensureKey(cashKey(currency))[i] = value;
            total += value;
        }
        totals[i] = total;
    }

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

PortfolioService::PortfolioMetadata PortfolioService::loadMetadata(const std::string& portfolioId) {
    auto snap = portfolioRepository_->loadLatestSnapshot(portfolioId);
    if (!snap.has_value()) throw std::runtime_error("No snapshot for portfolio: " + portfolioId);
    return {snap->portfolioId, snap->name, snap->baseCurrency};
}

std::vector<std::string> PortfolioService::listPortfolioIds() { return portfolioRepository_->listPortfolioIds(); }

std::vector<Transaction> PortfolioService::listTransactions(const std::string& portfolioId, int64_t afterTimestampMs) {
    return portfolioRepository_->loadTransactions(portfolioId, afterTimestampMs);
}

std::string PortfolioService::addTransaction(const std::string& portfolioId, Transaction transaction) {
    transaction.id = generateTransactionId();
    if (logger_)
        logger_->write(finapp::logging::Level::Info,
                       "addTransaction: portfolio='" + portfolioId + "' type=" + toString(transaction.type) +
                           " ticker=" + transaction.assetTicker + " qty=" + std::to_string(transaction.quantity));
    portfolioRepository_->appendTransactions(portfolioId, {transaction});
    rebuildSnapshotsFrom_(portfolioId, transaction.timestampsMs);
    return transaction.id;
}

std::vector<std::string> PortfolioService::importTransactions(const std::string& portfolioId,
                                                              std::vector<finance::Transaction> transactions) {
    if (logger_)
        logger_->write(finapp::logging::Level::Info, "importTransactions: portfolio='" + portfolioId + "' count=" +
                                                 std::to_string(transactions.size()));
    for (auto& t : transactions) t.id = generateTransactionId();
    std::sort(transactions.begin(), transactions.end(), [](const Transaction& a, const Transaction& b) {
        if (a.timestampsMs != b.timestampsMs) return a.timestampsMs < b.timestampsMs;
        return transactionTypePriority(a.type) < transactionTypePriority(b.type);
    });
    portfolioRepository_->appendTransactions(portfolioId, transactions);
    rebuildSnapshotsFrom_(portfolioId, transactions.front().timestampsMs);
    std::vector<std::string> ids;
    ids.reserve(transactions.size());
    for (const auto& t : transactions) ids.push_back(t.id);
    return ids;
}

void PortfolioService::deleteTransaction(const std::string& portfolioId, const std::string& transactionId) {
    if (logger_)
        logger_->write(finapp::logging::Level::Info,
                       "deleteTransaction: portfolio='" + portfolioId + "' txId='" + transactionId + "'");
    // Find the timestamp before deleting so we know where to start the rebuild.
    const auto allTxs = portfolioRepository_->loadTransactions(portfolioId, 0);
    const auto it =
        std::find_if(allTxs.begin(), allTxs.end(), [&](const Transaction& t) { return t.id == transactionId; });
    if (it == allTxs.end()) {
        throw std::runtime_error("PortfolioService::deleteTransaction: transaction '" + transactionId +
                                 "' not found in portfolio '" + portfolioId + "'.");
    }
    const int64_t deletedTs = it->timestampsMs;
    portfolioRepository_->deleteTransaction(portfolioId, transactionId);
    rebuildSnapshotsFrom_(portfolioId, deletedTs);
}

std::string PortfolioService::updateTransaction(const std::string& portfolioId, const std::string& transactionId,
                                                Transaction corrected) {
    deleteTransaction(portfolioId, transactionId);
    return addTransaction(portfolioId, std::move(corrected));
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void PortfolioService::rebuildSnapshotsFrom_(const std::string& portfolioId, int64_t fromTimestampMs) {
    if (logger_)
        logger_->write(finapp::logging::Level::Debug,
                       "rebuildSnapshotsFrom_: portfolio='" + portfolioId + "' from=" +
                           std::to_string(fromTimestampMs) + "ms");
    // Find the latest snapshot strictly before fromTimestampMs — this is our replay base.
    auto allSnapshots = portfolioRepository_->loadAllSnapshots(portfolioId);
    std::sort(allSnapshots.begin(), allSnapshots.end(), [](const PortfolioSnapshot& a, const PortfolioSnapshot& b) {
        return a.timestampMs < b.timestampMs;
    });

    std::optional<PortfolioSnapshot> baseSnap;
    for (auto it = allSnapshots.rbegin(); it != allSnapshots.rend(); ++it) {
        if (it->timestampMs < fromTimestampMs) {
            baseSnap = *it;
            break;
        }
    }
    if (!baseSnap) {
        throw std::runtime_error("PortfolioService::rebuildSnapshotsFrom_: no base snapshot before " +
                                 std::to_string(fromTimestampMs) + " for portfolio '" + portfolioId + "'.");
    }

    // Load every transaction that follows the base snapshot and sort them.
    auto allTxs = portfolioRepository_->loadTransactions(portfolioId, baseSnap->timestampMs + 1);
    std::sort(allTxs.begin(), allTxs.end(), [](const Transaction& a, const Transaction& b) {
        if (a.timestampsMs != b.timestampsMs) return a.timestampsMs < b.timestampsMs;
        return transactionTypePriority(a.type) < transactionTypePriority(b.type);
    });

    Portfolio portfolio =
        Portfolio::Builder(portfolioId, baseSnap->name, baseSnap->baseCurrency).fromSnapshot(*baseSnap).build();

    // Apply every transaction in chronological order (deposits before buys at
    // the same timestamp, thanks to transactionTypePriority sorting above).
    // Capture a snapshot after each transaction that falls at or after the
    // rebuild boundary — but keep only the *last* snapshot per unique timestamp
    // to avoid same-timestamp .pos/.cash file collisions.
    std::vector<PortfolioSnapshot> newSnapshots;
    newSnapshots.reserve(allTxs.size());
    for (const auto& tx : allTxs) {
        portfolio.apply(tx);
        if (tx.timestampsMs >= fromTimestampMs) {
            // Overwrite the previous entry if it has the same timestamp so that
            // the intermediate states of same-day transactions don't collide on
            // their side-car files.
            if (!newSnapshots.empty() && newSnapshots.back().timestampMs == tx.timestampsMs) {
                newSnapshots.back() = portfolio.snapshot(tx.timestampsMs);
            } else {
                newSnapshots.push_back(portfolio.snapshot(tx.timestampsMs));
            }
        }
    }

    // Single atomic write: trim stale rows and write all rebuilt snapshots in one pass.
    portfolioRepository_->replaceSnapshotsFrom(portfolioId, fromTimestampMs, newSnapshots);
}

void PortfolioService::recomputeAndCache_(const Portfolio&, int64_t, int64_t, int64_t) {
    // Intentionally empty — valueSeries/weightSeries already rely on TimeSeriesService
    // for caching the underlying market-data series, and re-walking the portfolio is
    // cheap compared to the fetch cost. Kept as a hook for future memoization.
}

}  // namespace finapp
