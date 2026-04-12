// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finapp/data/repository/interface/IPortfolioRepository.hpp"
#include "finapp/finance/portfolio/PortfolioSnapshot.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"
namespace finance {

class CSVPortfolioRepository : public IPortfolioRepository {
 public:
    explicit CSVPortfolioRepository(std::filesystem::path directory) : directory_{std::move(directory)} {};
    void saveSnapshot(const PortfolioSnapshot& snapshot) override;
    std::optional<PortfolioSnapshot> loadLatestSnapshot(const std::string& portfolio) const override;
    void appendTransactions(const std::string& portfolioId, const std::vector<Transaction>& transactions) override;
    std::vector<Transaction> loadTransactions(const std::string& portfolioId, int64_t afterTimestamps) const override;
    std::vector<std::string> listPortfolioIds() const override;
    bool exists(const std::string& portfolioID) const override;

 private:
    std::filesystem::path directory_;

    std::filesystem::path csvSnapshotPath_(const std::string& portfolioID) const;
    std::filesystem::path csvSnapshotPositionsPath_(const std::string& portfolioID, const int64_t& timestampMs) const;
    std::filesystem::path positionFilePath_(const std::string& positionsId) const;
    std::filesystem::path cashBalanceFilePath_(const std::string& cashBalancesID) const;
    std::filesystem::path csvSnapshotCashBalancesPath_(const std::string& portfolioID,
                                                       const int64_t& timestampMs) const;

    std::filesystem::path csvTransactionsPath_(const std::string& portfolioID) const;

    void writeSnapshotCsv_(const std::string& portfolioID, const PortfolioSnapshot& snapshot);
    std::string writeSnapshotPositionsFile_(const PortfolioSnapshot& snapshot);
    std::string writeCashBalancesFile_(const PortfolioSnapshot& snapshot);

    void writeFullTransactionsCsv_(const std::filesystem::path& path, const std::vector<Transaction>& transactions);
    void appendTransactionsCsv_(const std::filesystem::path& path, const std::vector<Transaction>& transactions);

    std::optional<PortfolioSnapshot> parseSnapshotCsvFile_(const std::filesystem::path& path) const;
    std::vector<SnapshotPosition> parsePositionsSnapshotFile_(const std::filesystem::path& path) const;
    std::unordered_map<Currency, double> parseCashBalanceFile_(const std::filesystem::path& path) const;
    std::vector<Transaction> parseTransactionsCsvFile_(const std::filesystem::path& path,
                                                       int64_t afterTimestamps) const;
};
}  // namespace finance
