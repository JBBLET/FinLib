// Copyright (c) 2026 JBBLET. All Rights Reserved.

#include "finapp/service/PortfolioService.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <limits>
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

#include "finapp/common/Error.hpp"
#include "finapp/common/logger/PrefixedLogger.hpp"
#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/common/AssetId.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finapp/finance/portfolio/PortfolioSnapshot.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/utils/TimeSeriesUtils.hpp"
#include "finlib/core/TimeSeries.hpp"

namespace finapp {

using finance::AssetId;
using finance::Currency;
using finance::Portfolio;
using finance::PortfolioSnapshot;
using finance::SnapshotPosition;
using finance::Transaction;

namespace {
constexpr Timestamp kDefaultSpotFrequencyMs = 86'400'000;

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
                                      Timestamp timestampMs) {
    ensure(!portfolioRepository_->exists(portfolioId), "PortfolioService::createNew: portfolio {} already exists.",
           portfolioId);
    if (logger_)
        logger_->write(finapp::logging::Level::Info,
                       "createNew: id='" + portfolioId + "' name='" + name + "' base=" + toString(baseCurrency));
    Portfolio portfolio = Portfolio::Builder(portfolioId, name, baseCurrency).build();
    portfolioRepository_->saveSnapshot(portfolio.snapshot(timestampMs - 1));
    return portfolio;
}

void PortfolioService::deletePortfolio(const std::string& portfolioId) {
    ensure(portfolioRepository_->exists(portfolioId), "PortfolioService::deletePortfolio: portfolio {} does not exist.", portfolioId);
    if (logger_) logger_->write(finapp::logging::Level::Info, "deletePortfolio: id='" + portfolioId + "'");
    portfolioRepository_->deletePortfolio(portfolioId);
}

Portfolio PortfolioService::load(const std::string& portfolioId) {
    if (logger_) logger_->write(finapp::logging::Level::Debug, "load: id='" + portfolioId + "'");
    auto snapshotOpt = portfolioRepository_->loadLatestSnapshot(portfolioId);
    ensure(snapshotOpt.has_value(), "PortfolioService::load: no snapshot found for portfolio {}", portfolioId);
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

void PortfolioService::save(const Portfolio& portfolio, Timestamp timestampMs) {
    portfolioRepository_->saveSnapshot(portfolio.snapshot(timestampMs));
}

// std::vector<Transaction> PortfolioService::rebalance(const Portfolio& portfolio, Timestamp timestampMs) {
//     const auto& targets = portfolio.targetAllocations();
//     if (targets.empty()) {
//         return {};
//     }
//     const Currency base = portfolio.baseCurrency();
//     const double total = totalValue(portfolio, timestampMs);
//
//     // Index current positions by ticker for O(1) delta lookup.
//     std::unordered_map<std::string, const SnapshotPosition*> currentByTicker;
//     currentByTicker.reserve(portfolio.positions().size());
//     for (const SnapshotPosition& pos : portfolio.positions()) {
//         currentByTicker[pos.assetId.ticker] = &pos;
//     }
//
//     std::vector<Transaction> transactions;
//     transactions.reserve(targets.size());
//
//     for (const TargetAllocation& target : targets) {
//         auto asset = assetService_->load(target.assetId);
//         const Currency denom = asset->denomination();
//
//         const double price =
//             assetService_->loadTimeSeriesValue(target.assetId, timestampMs, timestampMs, kDefaultSpotFrequencyMs)
//                 .getValues()
//                 .back();
//         double fx = 1.0;
//         if (denom != base) {
//             fx = fxService_->load(denom, base, timestampMs, timestampMs, kDefaultSpotFrequencyMs).getValues().back();
//         }
//
//         // Current value in base currency.
//         double currentValueBase = 0.0;
//         if (auto it = currentByTicker.find(target.assetId.ticker); it != currentByTicker.end()) {
//             currentValueBase = it->second->quantity * price * fx;
//         }
//         const double targetValueBase = target.weight * total;
//         const double deltaValueBase = targetValueBase - currentValueBase;
//
//         // Convert delta back into asset-denomination quantity.
//         if (price <= 0.0 || fx <= 0.0) {
//             throw finapp::Exception("PortfolioService::rebalance: non-positive price/fx for " +
//             target.assetId.ticker);
//         }
//         const double deltaQuantity = deltaValueBase / (price * fx);
//         if (deltaQuantity == 0.0) continue;
//
//         Transaction tx{};
//         tx.timestampsMs = timestampMs;
//         tx.type = deltaQuantity > 0.0 ? TransactionType::Buy : TransactionType::Sell;
//         tx.assetType = target.assetId.type;
//         tx.assetTicker = target.assetId.ticker;
//         tx.quantity = std::abs(deltaQuantity);
//         tx.pricePerUnit = price;
//         tx.fees = 0.0;
//         tx.settlementCurrency = denom;
//         transactions.push_back(tx);
//     }
//
//     return transactions;
// }

// ---------------------------------------------------------------------------
// Analysis grid — union/intersection of constituents' native observation ticks
// ---------------------------------------------------------------------------
TimestampsPtr PortfolioService::grid(const std::string& portfolioId, Timestamp startMs, Timestamp endMs,
                                     finance::GridMode mode) {
    auto snapshots = portfolioRepository_->loadSnapshotsCovering(portfolioId, startMs, endMs);
    if (snapshots.empty()) return std::make_shared<std::vector<Timestamp>>();
    const Currency base = snapshots.front().baseCurrency;

    std::unordered_set<AssetId> uniqueAssetIds;
    std::unordered_set<Currency> uniqueCurrencies;
    for (const PortfolioSnapshot& snap : snapshots) {
        for (const SnapshotPosition& pos : snap.positions) uniqueAssetIds.insert(pos.assetId);
        for (const auto& [c, _] : snap.cashBalances) uniqueCurrencies.insert(c);
    }

    std::vector<TimestampsPtr> grids;
    grids.reserve(uniqueAssetIds.size() + uniqueCurrencies.size());
    for (const AssetId& aid : uniqueAssetIds) {
        grids.push_back(assetService_->rawTicks(aid, startMs, endMs));
        uniqueCurrencies.insert(assetService_->load(aid)->denomination());  // fold in each denomination
    }
    for (Currency c : uniqueCurrencies) {
        if (c != base) grids.push_back(fxService_->rawTicks(c, base, startMs, endMs));
    }

    // Drop empty grids (cash, same-currency FX, unpriced assets) — an empty operand would
    // otherwise collapse an intersection to nothing.
    std::erase_if(grids, [](const TimestampsPtr& g) { return !g || g->empty(); });

    return (mode == finance::GridMode::Union) ? finance::unionOf(grids) : finance::intersectionOf(grids);
}

// ---------------------------------------------------------------------------
// Derived TimeSeries over a range
// ---------------------------------------------------------------------------
//
// This computes both total value and weight series it do so by processing segment between transactions
// via masks if the number of transactions grow need to change
finance::PortfolioSeries PortfolioService::valueAndWeightSeries(const std::string& portfolioId,
                                                                TimestampsPtr timestamps) {
    ensure<InvalidArgument>(timestamps && !timestamps->empty(),
                            "PortfolioService::valueSeries: timestamps must be non-empty.");
    auto snapshots = portfolioRepository_->loadSnapshotsCovering(portfolioId, timestamps->front(), timestamps->back());
    if (snapshots.empty()) {
        return {ts::common::utils::timeSeries::generateConstantTimeSeries(portfolioId + "_totalValue", timestamps, 0.0),
                {}};
    }
    std::sort(snapshots.begin(), snapshots.end(), [](const PortfolioSnapshot& a, const PortfolioSnapshot& b) {
        return a.timestampMs < b.timestampMs;
    });
    auto baseCurrency = snapshots.front().baseCurrency;

    // Collect every asset and currency that appears in any snapshot.
    std::unordered_set<AssetId> uniqueAssetIds;
    std::unordered_set<Currency> uniqueCurrencies;

    for (const PortfolioSnapshot& snap : snapshots) {
        for (const SnapshotPosition& pos : snap.positions) uniqueAssetIds.insert(pos.assetId);
        for (const auto& [c, _] : snap.cashBalances) uniqueCurrencies.insert(c);
    }

    std::unordered_map<finance::Currency, TimeSeries> fxCache;
    std::unordered_map<AssetId, TimeSeries> priceInBase;

    priceInBase.reserve(uniqueAssetIds.size() + uniqueCurrencies.size());
    for (Currency c : uniqueCurrencies) {
        fxCache[c] = (c != baseCurrency)
                         ? fxService_->load(c, baseCurrency, timestamps)
                         : ts::common::utils::timeSeries::generateConstantTimeSeries("fx", timestamps, 1.0);
    }

    for (const AssetId& aid : uniqueAssetIds) {
        auto asset = assetService_->load(aid);
        auto assetDenom = asset->denomination();
        if (assetDenom == baseCurrency) {
            priceInBase.emplace(aid, assetService_->loadTimeSeriesValue(aid, timestamps));
        } else {
            auto [iterator, notPreviouslyPresent] = uniqueCurrencies.insert(assetDenom);
            if (notPreviouslyPresent) {
                fxCache.emplace(assetDenom, fxService_->load(assetDenom, baseCurrency, timestamps));
            }
            auto price = assetService_->loadTimeSeriesValue(aid, timestamps) * fxCache.at(assetDenom);
            priceInBase.emplace(aid, price);
        }
    }
    auto totalAccumulation =
        ts::common::utils::timeSeries::generateConstantTimeSeries(portfolioId + "_value", timestamps, 0.0);
    std::unordered_map<AssetId, TimeSeries> weightAccumulation;
    for (size_t i = 0; i < snapshots.size(); i++) {
        auto tsSnapshot = snapshots[i].timestampMs;
        auto nextTs = (i + 1 < snapshots.size()) ? snapshots[i + 1].timestampMs : +std::numeric_limits<int64_t>::max();
        auto portfolioSegment =
            Portfolio::Builder(portfolioId, snapshots[i].name, baseCurrency).fromSnapshot(snapshots[i]).build();
        auto segmentSeries = portfolioSegment.valueAndWeightSeries(timestamps, priceInBase, fxCache);
        auto mask = ts::common::utils::timeSeries::makeSegmentMask(timestamps, tsSnapshot, nextTs);
        totalAccumulation += segmentSeries.total * mask;

        for (const auto& [assetId, weightSeries] : segmentSeries.weights) {
            if (!weightAccumulation.contains(assetId)) {
                weightAccumulation[assetId] =
                    ts::common::utils::timeSeries::generateConstantTimeSeries(assetId.ticker, timestamps, 0.0);
            }
            weightAccumulation[assetId] += weightSeries * mask;
        }
    }

    return {totalAccumulation, weightAccumulation};
}

finance::PortfolioSeries PortfolioService::valueAndWeightSeries(const std::string& portfolioId, Timestamp startMs,
                                                                Timestamp endMs, Timestamp frequencyMs) {
    return valueAndWeightSeries(portfolioId,
                                ts::common::utils::timeSeries::makeRegularTimestamps(startMs, endMs, frequencyMs));
}

std::unordered_map<AssetId, TimeSeries> PortfolioService::quantitySeries(const std::string& portfolioId,
                                                                         TimestampsPtr timestamps) {
    ensure<InvalidArgument>(timestamps && !timestamps->empty(),
                            "PortfolioService::valueSeries: timestamps must be non-empty.");
    ensure<InvalidArgument>(std::is_sorted(timestamps->begin(), timestamps->end()), "Timestamps must be sorted");
    auto snapshots = portfolioRepository_->loadSnapshotsCovering(portfolioId, timestamps->front(), timestamps->back());
    ensure(!snapshots.empty(), "PortfolioService::valueSeries: no snapshot for portfolio {}", portfolioId);
    std::sort(snapshots.begin(), snapshots.end(), [](const PortfolioSnapshot& a, const PortfolioSnapshot& b) {
        return a.timestampMs < b.timestampMs;
    });

    // Reserve space for maps
    std::unordered_set<AssetId> uniqueAssetIds;
    std::unordered_set<Currency> uniqueCurrencies;
    for (const PortfolioSnapshot& snap : snapshots) {
        for (const SnapshotPosition& pos : snap.positions) uniqueAssetIds.insert(pos.assetId);
        for (const auto& [c, _] : snap.cashBalances) uniqueCurrencies.insert(c);
    }

    std::unordered_map<AssetId, std::vector<std::pair<Timestamp, double>>> assetQuantities;
    assetQuantities.reserve(uniqueAssetIds.size());
    std::unordered_map<Currency, std::vector<std::pair<Timestamp, double>>> cashQuantities;
    cashQuantities.reserve(uniqueCurrencies.size());
    for (const PortfolioSnapshot& snap : snapshots) {
        for (const SnapshotPosition& pos : snap.positions) {
            assetQuantities[pos.assetId].reserve(snapshots.size());
        }
        for (const auto& [currency, balance] : snap.cashBalances) {
            cashQuantities[currency].reserve(snapshots.size());
        }
    }

    for (const PortfolioSnapshot& snap : snapshots) {
        auto ts = std::lower_bound(timestamps->begin(), timestamps->end(), snap.timestampMs);
        if (ts != timestamps->end()) {
            for (const auto& assetId : uniqueAssetIds) {
                assetQuantities.at(assetId).push_back({*ts, 0.0});
            }
            for (const auto& currency : uniqueCurrencies) {
                cashQuantities.at(currency).push_back({*ts, 0.0});
            }
            for (const SnapshotPosition& pos : snap.positions) {
                assetQuantities.at(pos.assetId).back() = {*ts, pos.quantity};
            }
            for (const auto& [currency, balance] : snap.cashBalances) {
                cashQuantities.at(currency).back() = {*ts, balance};
            }
        } else {
            break;
        }
    }

    std::unordered_map<AssetId, TimeSeries> output;
    output.reserve(assetQuantities.size());
    for (const auto& [assetId, quantities] : assetQuantities) {
        output[assetId] =
            ts::common::utils::timeSeries::generateStepSeries(assetId.ticker + "_quantity", quantities, timestamps);
    }
    for (const auto& [currency, quantities] : cashQuantities) {
        output[AssetId{finance::AssetType::Cash, finance::toString(currency)}] =
            ts::common::utils::timeSeries::generateStepSeries(
                finance::toString(currency) + "_quantity", quantities, timestamps);
    }
    return output;
}

std::unordered_map<AssetId, TimeSeries> PortfolioService::quantitySeries(const std::string& portfolioId,
                                                                         Timestamp startMs, Timestamp endMs,
                                                                         Timestamp frequencyMs) {
    return quantitySeries(portfolioId,
                          ts::common::utils::timeSeries::makeRegularTimestamps(startMs, endMs, frequencyMs));
}

TimeSeries PortfolioService::valueSeries(const std::string& portfolioId, TimestampsPtr timestamps) {
    ensure<InvalidArgument>(timestamps && !timestamps->empty(),
                            "PortfolioService::valueSeries: timestamps must be non-empty.");
    auto snapshots = portfolioRepository_->loadSnapshotsCovering(portfolioId, timestamps->front(), timestamps->back());
    if (snapshots.empty()) {
        return ts::common::utils::timeSeries::generateConstantTimeSeries(portfolioId + "_totalValue", timestamps, 0.0);
    }
    std::sort(snapshots.begin(), snapshots.end(), [](const PortfolioSnapshot& a, const PortfolioSnapshot& b) {
        return a.timestampMs < b.timestampMs;
    });
    auto baseCurrency = snapshots.front().baseCurrency;

    // Collect every asset and currency that appears in any snapshot.
    std::unordered_set<AssetId> uniqueAssetIds;
    std::unordered_set<Currency> uniqueCurrencies;

    for (const PortfolioSnapshot& snap : snapshots) {
        for (const SnapshotPosition& pos : snap.positions) uniqueAssetIds.insert(pos.assetId);
        for (const auto& [c, _] : snap.cashBalances) uniqueCurrencies.insert(c);
    }

    std::unordered_map<finance::Currency, TimeSeries> fxCache;
    std::unordered_map<AssetId, TimeSeries> priceInBase;

    priceInBase.reserve(uniqueAssetIds.size() + uniqueCurrencies.size());
    for (Currency c : uniqueCurrencies) {
        fxCache[c] = (c != baseCurrency)
                         ? fxService_->load(c, baseCurrency, timestamps)
                         : ts::common::utils::timeSeries::generateConstantTimeSeries("fx", timestamps, 1.0);
    }

    for (const AssetId& aid : uniqueAssetIds) {
        auto asset = assetService_->load(aid);
        auto assetDenom = asset->denomination();
        if (assetDenom == baseCurrency) {
            priceInBase.emplace(aid, assetService_->loadTimeSeriesValue(aid, timestamps));
        } else {
            auto [iterator, notPreviouslyPresent] = uniqueCurrencies.insert(assetDenom);
            if (notPreviouslyPresent) {
                fxCache.emplace(assetDenom, fxService_->load(assetDenom, baseCurrency, timestamps));
            }
            auto price = assetService_->loadTimeSeriesValue(aid, timestamps) * fxCache.at(assetDenom);
            priceInBase.emplace(aid, price);
        }
    }
    auto totalAccumulation =
        ts::common::utils::timeSeries::generateConstantTimeSeries(portfolioId + "_value", timestamps, 0.0);

    for (size_t i = 0; i < snapshots.size(); i++) {
        auto tsSnapshot = snapshots[i].timestampMs;
        auto nextTs = (i + 1 < snapshots.size()) ? snapshots[i + 1].timestampMs : +std::numeric_limits<int64_t>::max();
        auto portfolioSegment =
            Portfolio::Builder(portfolioId, snapshots[i].name, baseCurrency).fromSnapshot(snapshots[i]).build();
        auto segmentSeries = portfolioSegment.valueSeries(timestamps, priceInBase, fxCache);
        auto mask = ts::common::utils::timeSeries::makeSegmentMask(timestamps, tsSnapshot, nextTs);
        totalAccumulation += segmentSeries * mask;
    }

    return totalAccumulation;
}
TimeSeries PortfolioService::valueSeries(const std::string& portfolioId, Timestamp startMs, Timestamp endMs,
                                         Timestamp frequencyMs) {
    return valueSeries(portfolioId, ts::common::utils::timeSeries::makeRegularTimestamps(startMs, endMs, frequencyMs));
}

std::unordered_map<AssetId, TimeSeries> PortfolioService::weightsSeries(const std::string& portfolioId,
                                                                        TimestampsPtr timestamps) {
    return valueAndWeightSeries(portfolioId, timestamps).weights;
}

std::unordered_map<AssetId, TimeSeries> PortfolioService::weightsSeries(const std::string& portfolioId,
                                                                        Timestamp startMs, Timestamp endMs,
                                                                        Timestamp frequencyMs) {
    return valueAndWeightSeries(portfolioId,
                                ts::common::utils::timeSeries::makeRegularTimestamps(startMs, endMs, frequencyMs))
        .weights;
}

finance::PortfolioOverviewAtTs PortfolioService::computeOverviewAtTs(const std::string& portfolioId, Timestamp ts) {
    const auto recentSnapshot = portfolioRepository_->loadClosestSnapshot(portfolioId, ts);
    if (!recentSnapshot.has_value()) {
        std::unordered_map<std::string, double> weights;
        return {ts, 0.0, weights};
    }
    return computePortfolioSnapshotAtSpecificTs_(recentSnapshot.value(), ts);
}
// ---------------------------------------------------------------------------
// Listing and transaction management
// ---------------------------------------------------------------------------

PortfolioService::PortfolioMetadata PortfolioService::loadMetadata(const std::string& portfolioId) {
    auto snap = portfolioRepository_->loadLatestSnapshot(portfolioId);
    ensure(snap.has_value(), "No snapshot for portfolio: {}", portfolioId);
    return {snap->portfolioId, snap->name, snap->baseCurrency};
}

std::vector<std::string> PortfolioService::listPortfolioIds() { return portfolioRepository_->listPortfolioIds(); }

std::vector<Transaction> PortfolioService::listTransactions(const std::string& portfolioId,
                                                            Timestamp afterTimestampMs) {
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
        logger_->write(
            finapp::logging::Level::Info,
            "importTransactions: portfolio='" + portfolioId + "' count=" + std::to_string(transactions.size()));
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
    ensure(it != allTxs.end(), "PortfolioService::deleteTransaction: transaction {} not found in portfolio {}.",
           transactionId, portfolioId);
    const Timestamp deletedTs = it->timestampsMs;
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

void PortfolioService::rebuildSnapshotsFrom_(const std::string& portfolioId, Timestamp fromTimestampMs) {
    if (logger_)
        logger_->write(
            finapp::logging::Level::Debug,
            "rebuildSnapshotsFrom_: portfolio='" + portfolioId + "' from=" + std::to_string(fromTimestampMs) + "ms");
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
    ensure(baseSnap.has_value(),
           "PortfolioService::rebuildSnapshotsFrom_: no base snapshot before {} for portfolio {}.", fromTimestampMs,
           portfolioId);

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

finance::PortfolioOverviewAtTs PortfolioService::computePortfolioSnapshotAtSpecificTs_(
    const PortfolioSnapshot& snapshot, Timestamp ts) {
    const Currency baseCurrency = snapshot.baseCurrency;

    std::unordered_map<const Currency, std::vector<std::string>> assetToConvert;
    assetToConvert.reserve(finance::valid_currencies.size());
    std::unordered_map<std::string, double> valueByTicker;
    valueByTicker.reserve(snapshot.positions.size() + snapshot.cashBalances.size());
    double total = 0.0;

    // Iterate over positions
    for (const SnapshotPosition& pos : snapshot.positions) {
        if (pos.quantity == 0.0) continue;
        auto asset = assetService_->load(pos.assetId);
        const Currency denom = asset->denomination();
        const double price = assetService_->loadValueAtTs(pos.assetId, ts);
        assetToConvert[denom].push_back(pos.assetId.ticker);
        valueByTicker[pos.assetId.ticker] = price * pos.quantity;
    }
    for (auto& [currency, tickers] : assetToConvert) {
        double fx = (currency != baseCurrency)
                        ? fxService_->loadSingleFxAtTs(currency, baseCurrency, ts)  // asset → base
                        : 1.0;
        for (const auto& ticker : tickers) {
            valueByTicker[ticker] *= fx;
            total += valueByTicker[ticker];
        }
    }
    // Cash balances
    for (const auto& [currency, amount] : snapshot.cashBalances) {
        double fx = (currency != baseCurrency) ? fxService_->loadSingleFxAtTs(currency, baseCurrency, ts) : 1.0;
        const double valueBase = amount * fx;
        valueByTicker[cashKey(currency)] = valueBase;
        total += valueBase;
    }
    if (total <= 0.0) return {ts, 0.0, {}};

    std::unordered_map<std::string, double> weightsByTicker;
    weightsByTicker.reserve(valueByTicker.size());
    for (const auto& [ticker, value] : valueByTicker) weightsByTicker[ticker] = value / total;

    return finance::PortfolioOverviewAtTs{ts, total, std::move(weightsByTicker)};
}

void PortfolioService::recomputeAndCache_(const Portfolio&, Timestamp, Timestamp, Timestamp) {
    // Intentionally empty — valueSeries already relies on TimeSeriesService
    // for caching the underlying market-data series, and re-walking the portfolio is
    // cheap compared to the fetch cost. Kept as a hook for future memoization.
}

}  // namespace finapp
