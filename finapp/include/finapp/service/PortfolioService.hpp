// Copyright (c) 2026 JBBLET. All Rights Reserved.

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "finapp/common/logger/ILogger.hpp"
#include "finapp/data/repository/interface/IPortfolioRepository.hpp"
#include "finapp/finance/common/AssetId.hpp"
#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finapp/finance/portfolio/PortfolioSnapshot.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"
#include "finapp/service/AssetService.hpp"
#include "finapp/service/FXService.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/core/TimeSeries.hpp"

using ts::InterpolationStrategy;
using ts::TimeSeries;
using ts::TimeSeriesService;
using ts::Timestamp;
using ts::Timestamps;
using ts::TimestampsPtr;
using ts::analysis::TimeSeriesSession;

namespace finapp {

class PortfolioService {
 public:
    PortfolioService(std::shared_ptr<IPortfolioRepository> portfolioRepository,
                     std::shared_ptr<AssetService> assetService, std::shared_ptr<FXService> fxService,
                     finapp::logging::ILogger* logger = nullptr);

    // Create and persist a new empty portfolio. Seeds an empty snapshot so load()
    // works immediately. Throws if a portfolio with that id already exists.
    finance::Portfolio createNew(const std::string& portfolioId, const std::string& name,
                                 finance::Currency baseCurrency, Timestamp timestampMs = 0);

    // Reconstruct from snapshot + transactions
    finance::Portfolio load(const std::string& portfolioId);

    // Soft-delete: delegates to the repository. The portfolio can no longer be
    // loaded or listed but its data files are preserved on disk.
    // Throws if the portfolio does not exist.
    void deletePortfolio(const std::string& portfolioId);

    // Persist snapshot + append new transactions
    void save(const finance::Portfolio& portfolio, Timestamp timestampMs);

    struct PortfolioMetadata {
        std::string id;
        std::string name;
        finance::Currency baseCurrency;
    };

    // Returns id/name/baseCurrency from the latest snapshot only.
    // Does NOT replay transactions or call AssetService/FXService.
    PortfolioMetadata loadMetadata(const std::string& portfolioId);

    // Listing
    std::vector<std::string> listPortfolioIds();
    std::vector<finance::Transaction> listTransactions(const std::string& portfolioId, Timestamp afterTimestampMs = 0);

    // Assigns a generated id, applies the transaction, appends it to the log, and saves a snapshot.
    // Returns the generated id — the only information the caller didn't already have.
    std::string addTransaction(const std::string& portfolioId, finance::Transaction transaction);

    // Bulk variant: assigns IDs to all transactions, applies them, appends them in one write,
    // and saves a single snapshot. More efficient than calling addTransaction in a loop.
    // Returns the generated ids in the same order as the input.
    std::vector<std::string> importTransactions(const std::string& portfolioId,
                                                std::vector<finance::Transaction> transactions);

    // Remove a transaction by id and invalidate snapshots at or after its timestamp.
    // The portfolio will be rebuilt from the preceding snapshot on next load().
    void deleteTransaction(const std::string& portfolioId, const std::string& transactionId);

    // Convenience: delete the old transaction then re-add the corrected one.
    // Returns the new generated id assigned to the corrected transaction.
    std::string updateTransaction(const std::string& portfolioId, const std::string& transactionId,
                                  finance::Transaction corrected);

    // Compute the Overview at a specific Timestamp in a single Passage.
    finance::PortfolioOverviewAtTs computeOverviewAtTs(const std::string& portfolioId, Timestamp ts);

    // Computiation of TimeSeries
    finance::PortfolioSeries valueAndWeightSeries(const std::string& id, TimestampsPtr timestamps);

    finance::PortfolioSeries valueAndWeightSeries(const std::string& id, Timestamp startMs, Timestamp endMs,
                                                  Timestamp frequencyMs);

    std::unordered_map<finance::AssetId, TimeSeries> quantitySeries(const std::string& portfolioId,
                                                                    TimestampsPtr timestamps);

    std::unordered_map<finance::AssetId, TimeSeries> quantitySeries(const std::string& portfolioId, Timestamp startMs,
                                                                    Timestamp endMs, Timestamp frequencyMs);

    TimeSeries valueSeries(const std::string& portfolioId, TimestampsPtr timestamps);

    TimeSeries valueSeries(const std::string& portfolioId, Timestamp startMs, Timestamp endMs, Timestamp frequencyMs);

    std::unordered_map<finance::AssetId, TimeSeries> weightsSeries(const std::string& portfolioId,
                                                                   TimestampsPtr timestamps);

    std::unordered_map<finance::AssetId, TimeSeries> weightsSeries(const std::string& portfolioId, Timestamp startMs,
                                                                   Timestamp endMs, Timestamp frequencyMs);

 private:
    std::shared_ptr<IPortfolioRepository> portfolioRepository_;
    std::shared_ptr<AssetService> assetService_;
    std::shared_ptr<FXService> fxService_;
    std::unique_ptr<finapp::logging::ILogger> logger_;

    void recomputeAndCache_(const finance::Portfolio& portfolio, Timestamp fromMs, Timestamp toMs,
                            Timestamp frequencyMs);
    void rebuildSnapshotsFrom_(const std::string& portfolioId, Timestamp fromTimestampMs);

    finance::PortfolioOverviewAtTs computePortfolioSnapshotAtSpecificTs_(const finance::PortfolioSnapshot& snapshot,
                                                                         Timestamp ts);
};
}  // namespace finapp
