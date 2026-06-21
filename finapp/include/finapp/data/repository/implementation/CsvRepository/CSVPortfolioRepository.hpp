// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "finapp/common/logger/ILogger.hpp"
#include "finapp/data/repository/interface/IPortfolioRepository.hpp"
#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finapp/finance/portfolio/PortfolioSnapshot.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"

using Timestamp = int64_t;
namespace finapp {

class CSVPortfolioRepository : public IPortfolioRepository {
 public:
    explicit CSVPortfolioRepository(std::filesystem::path directory, finapp::logging::ILogger* logger = nullptr);
    void saveSnapshot(const finance::PortfolioSnapshot& snapshot) override;
    std::optional<finance::PortfolioSnapshot> loadLatestSnapshot(const std::string& portfolio) const override;
    std::vector<finance::PortfolioSnapshot> loadAllSnapshots(const std::string& portfolioId) const override;
    std::optional<finance::PortfolioSnapshot> loadClosestSnapshot(const std::string& portfolioId,
                                                                  const Timestamp& ts) const override;
    void replaceSnapshotsFrom(const std::string& portfolioId, int64_t fromTimestampMs,
                              const std::vector<finance::PortfolioSnapshot>& newSnapshots) override;
    void appendTransactions(const std::string& portfolioId,
                            const std::vector<finance::Transaction>& transactions) override;
    std::vector<finance::Transaction> loadTransactions(const std::string& portfolioId,
                                                       int64_t afterTimestamps) const override;
    std::vector<std::string> listPortfolioIds() const override;
    bool exists(const std::string& portfolioID) const override;
    void deletePortfolio(const std::string& portfolioId) override;
    void deleteTransaction(const std::string& portfolioId, const std::string& transactionId) override;

 private:
    std::filesystem::path directory_;
    std::unique_ptr<finapp::logging::ILogger> logger_;

    std::filesystem::path csvSnapshotPath_(const std::string& portfolioID) const;
    std::filesystem::path csvSnapshotPositionsPath_(const std::string& portfolioID, const int64_t& timestampMs) const;
    std::filesystem::path positionFilePath_(const std::string& positionsId) const;
    std::filesystem::path cashBalanceFilePath_(const std::string& cashBalancesID) const;
    std::filesystem::path csvSnapshotCashBalancesPath_(const std::string& portfolioID,
                                                       const int64_t& timestampMs) const;

    std::filesystem::path csvTransactionsPath_(const std::string& portfolioID) const;
    std::filesystem::path csvDeletedSnapshotPath_(const std::string& portfolioID) const;
    std::filesystem::path csvDeletedTransactionsPath_(const std::string& portfolioID) const;

    void writeSnapshotCsv_(const std::string& portfolioID, const finance::PortfolioSnapshot& snapshot);
    void trimSnapshotRowsFrom_(const std::string& portfolioId, int64_t fromTimestampMs);
    std::string writeSnapshotPositionsFile_(const finance::PortfolioSnapshot& snapshot);
    std::string writeCashBalancesFile_(const finance::PortfolioSnapshot& snapshot);

    void writeFullTransactionsCsv_(const std::filesystem::path& path,
                                   const std::vector<finance::Transaction>& transactions);
    void appendTransactionsCsv_(const std::filesystem::path& path,
                                const std::vector<finance::Transaction>& transactions);

    std::optional<finance::PortfolioSnapshot> parseSnapshotCsvFile_(const std::filesystem::path& path) const;
    std::vector<finance::PortfolioSnapshot> parseAllSnapshotRows_(const std::filesystem::path& path) const;
    std::vector<finance::SnapshotPosition> parsePositionsSnapshotFile_(const std::filesystem::path& path) const;
    std::unordered_map<finance::Currency, double> parseCashBalanceFile_(const std::filesystem::path& path) const;
    std::vector<finance::Transaction> parseTransactionsCsvFile_(const std::filesystem::path& path,
                                                                Timestamp afterTimestamps) const;
};
}  // namespace finapp
