// Copyright (c) 2026 JBBLET. All Rights Reserved.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "finapp/data/repository/interface/IPortfolioRepository.hpp"
#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"
#include "finapp/service/AssetService.hpp"
#include "finapp/service/FXService.hpp"
#include "finlib/core/TimeSeries.hpp"

namespace finapp {

class PortfolioService {
 public:
    PortfolioService(std::shared_ptr<IPortfolioRepository> portfolioRepository,
                     std::shared_ptr<AssetService> assetService, std::shared_ptr<FXService> fxService);

    // Create and persist a new empty portfolio. Seeds an empty snapshot so load()
    // works immediately. Throws if a portfolio with that id already exists.
    finance::Portfolio createNew(const std::string& portfolioId, const std::string& name, finance::Currency baseCurrency,
                        int64_t timestampMs);

    // Reconstruct from snapshot + transactions
    finance::Portfolio load(const std::string& portfolioId);

    // Soft-delete: delegates to the repository. The portfolio can no longer be
    // loaded or listed but its data files are preserved on disk.
    // Throws if the portfolio does not exist.
    void deletePortfolio(const std::string& portfolioId);

    // Persist snapshot + append new transactions
    void save(const finance::Portfolio& portfolio, int64_t timestampMs);

    struct PortfolioMetadata {
        std::string       id;
        std::string       name;
        finance::Currency baseCurrency;
    };

    // Returns id/name/baseCurrency from the latest snapshot only.
    // Does NOT replay transactions or call AssetService/FXService.
    PortfolioMetadata loadMetadata(const std::string& portfolioId);

    // Listing
    std::vector<std::string> listPortfolioIds();
    std::vector<finance::Transaction> listTransactions(const std::string& portfolioId, int64_t afterTimestampMs = 0);

    // Assigns a generated id, applies the transaction, appends it to the log, and saves a snapshot.
    // Returns the generated id — the only information the caller didn't already have.
    std::string addTransaction(const std::string& portfolioId, finance::Transaction transaction, int64_t timestampMs);

    // Remove a transaction by id and invalidate snapshots at or after its timestamp.
    // The portfolio will be rebuilt from the preceding snapshot on next load().
    void deleteTransaction(const std::string& portfolioId, const std::string& transactionId);

    // Convenience: delete the old transaction then re-add the corrected one.
    // Returns the new generated id assigned to the corrected transaction.
    std::string updateTransaction(const std::string& portfolioId, const std::string& transactionId,
                                  finance::Transaction corrected, int64_t timestampMs);

    // Market-data-dependent computations (fetches prices/FX internally)
    double totalValue(const finance::Portfolio& portfolio, int64_t timestampMs);
    std::unordered_map<std::string, double> weights(const finance::Portfolio& portfolio, int64_t timestampMs);
    std::vector<finance::Transaction> rebalance(const finance::Portfolio& portfolio, int64_t timestampMs);

    // Derived TimeSeries over a range
    TimeSeries valueSeries(const std::string& portfolioId, int64_t startMs, int64_t endMs, int64_t frequencyMs);
    std::unordered_map<std::string, TimeSeries> weightSeries(const std::string& portfolioId, int64_t startMs,
                                                             int64_t endMs, int64_t frequencyMs);

    // Shared-timestamp overloads — asset/FX series stay pointer-aligned on the caller's grid.
    TimeSeries valueSeries(const std::string& portfolioId, TimestampPtr timestamps);
    std::unordered_map<std::string, TimeSeries> weightSeries(const std::string& portfolioId, TimestampPtr timestamps);

 private:
    std::shared_ptr<IPortfolioRepository> portfolioRepository_;
    std::shared_ptr<AssetService> assetService_;
    std::shared_ptr<FXService> fxService_;

    void recomputeAndCache_(const finance::Portfolio& portfolio, int64_t fromMs, int64_t toMs, int64_t frequencyMs);
};
}  // namespace finapp
