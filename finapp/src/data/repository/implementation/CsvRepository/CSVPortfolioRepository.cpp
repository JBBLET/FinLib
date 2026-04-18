// Copyright (c) 2026 JBBLET. All Rights Reserved.

#include "finapp/data/repository/implementation/CsvRepository/CSVPortfolioRepository.hpp"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/common/AssetId.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finapp/finance/portfolio/PortfolioSnapshot.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"

namespace finapp {

using namespace finance;

// IPortfolioRepository Interface
void CSVPortfolioRepository::saveSnapshot(const PortfolioSnapshot& snapshot) {
    writeSnapshotCsv_(snapshot.portfolioId, snapshot);
}

std::optional<PortfolioSnapshot> CSVPortfolioRepository::loadLatestSnapshot(const std::string& portfolioId) const {
    auto path = csvSnapshotPath_(portfolioId);
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }
    auto result = parseSnapshotCsvFile_(path);
    return result;
}

void CSVPortfolioRepository::appendTransactions(const std::string& portfolioID,
                                                const std::vector<Transaction>& transactions) {
    auto path = csvTransactionsPath_(portfolioID);
    std::filesystem::create_directories(path.parent_path());
    bool needsheader = !std::filesystem::exists(path) || std::filesystem::file_size(path) == 0;
    if (needsheader) {
        writeFullTransactionsCsv_(path, transactions);
    } else {
        std::vector<Transaction> existingTransaction = parseTransactionsCsvFile_(path, 0);
        std::unordered_set<Transaction> existingSet(existingTransaction.begin(), existingTransaction.end());
        std::vector<Transaction> transactionsNotSaved;
        std::copy_if(
            transactions.begin(), transactions.end(), std::back_inserter(transactionsNotSaved),
            [&](const Transaction& transaction) { return existingSet.find(transaction) == existingSet.end(); });
        appendTransactionsCsv_(path, transactionsNotSaved);
    }
}

std::vector<Transaction> CSVPortfolioRepository::loadTransactions(const std::string& portfolioId,
                                                                  int64_t afterTimestamps) const {
    std::vector<Transaction> result;
    auto path = csvTransactionsPath_(portfolioId);
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Transactions CSV File not found: " + path.string());
    }
    result = std::move(parseTransactionsCsvFile_(path, afterTimestamps));
    return result;
}

std::vector<std::string> CSVPortfolioRepository::listPortfolioIds() const {
    std::filesystem::path path = directory_ / "Portfolio";
    std::vector<std::string> output;
    output.reserve(100);
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.path().extension() == ".csv") {
            std::string fileName = entry.path().stem();
            size_t positions = fileName.find("_snapshot");
            if (positions != std::string::npos) {
                fileName.erase(positions, 9);
                output.push_back(fileName);
            }
        }
    }
    return output;
}

bool CSVPortfolioRepository::exists(const std::string& portfolioId) const {
    return std::filesystem::exists(csvSnapshotPath_(portfolioId)) ||
           std::filesystem::exists(csvTransactionsPath_(portfolioId));
}

void CSVPortfolioRepository::deletePortfolio(const std::string& portfolioId) {
    const auto snapshotPath     = csvSnapshotPath_(portfolioId);
    const auto transactionsPath = csvTransactionsPath_(portfolioId);

    if (!std::filesystem::exists(snapshotPath) && !std::filesystem::exists(transactionsPath)) {
        throw std::runtime_error("CSVPortfolioRepository::deletePortfolio: portfolio '" + portfolioId +
                                 "' does not exist.");
    }
    if (std::filesystem::exists(snapshotPath)) {
        std::filesystem::rename(snapshotPath, csvDeletedSnapshotPath_(portfolioId));
    }
    if (std::filesystem::exists(transactionsPath)) {
        std::filesystem::rename(transactionsPath, csvDeletedTransactionsPath_(portfolioId));
    }
}

// Helper functions
std::filesystem::path CSVPortfolioRepository::csvSnapshotPath_(const std::string& portfolioID) const {
    return directory_ / "Portfolio" / (portfolioID + "_snapshot.csv");
}

std::filesystem::path CSVPortfolioRepository::csvTransactionsPath_(const std::string& portfolioID) const {
    return directory_ / "Portfolio" / (portfolioID + "_transactions.csv");
}

std::filesystem::path CSVPortfolioRepository::csvDeletedSnapshotPath_(const std::string& portfolioID) const {
    return directory_ / "Portfolio" / (portfolioID + "_snapshot.csv.deleted");
}

std::filesystem::path CSVPortfolioRepository::csvDeletedTransactionsPath_(const std::string& portfolioID) const {
    return directory_ / "Portfolio" / (portfolioID + "_transactions.csv.deleted");
}

std::filesystem::path CSVPortfolioRepository::csvSnapshotPositionsPath_(const std::string& portfolioID,
                                                                        const int64_t& timestampMs) const {
    return directory_ / "Portfolio" / "Positions" / (portfolioID + "_" + std::to_string(timestampMs) + ".pos");
}

std::filesystem::path CSVPortfolioRepository::csvSnapshotCashBalancesPath_(const std::string& portfolioID,
                                                                           const int64_t& timestampMs) const {
    return directory_ / "Portfolio" / "Cash" / (portfolioID + "_" + std::to_string(timestampMs) + ".cash");
}
std::filesystem::path CSVPortfolioRepository::positionFilePath_(const std::string& positionsId) const {
    return directory_ / "Portfolio" / "Positions" / (positionsId + ".pos");
}

std::filesystem::path CSVPortfolioRepository::cashBalanceFilePath_(const std::string& cashBalanceId) const {
    return directory_ / "Portfolio" / "Cash" / (cashBalanceId + ".cash");
}

void CSVPortfolioRepository::writeSnapshotCsv_(const std::string& portfolioID, const PortfolioSnapshot& snapshot) {
    auto path = csvSnapshotPath_(portfolioID);
    std::filesystem::create_directories(path.parent_path());
    bool needsheader = !std::filesystem::exists(path) || std::filesystem::file_size(path) == 0;

    std::ofstream file(path, std::ios::app);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open CSV file of snapshot for writing: " + path.string());
    }
    if (needsheader) {
        file << "name,baseCurrency,timestampMs,portfolioID,positions_id,cashBalances_id\n";
    }
    std::string timestampMsString = std::to_string(snapshot.timestampMs);
    std::string cashBalancesId = "";
    std::string positionsId = "";
    if (snapshot.positions.size() > 0) {
        positionsId = writeSnapshotPositionsFile_(snapshot);
    }
    if (snapshot.cashBalances.size() > 0) {
        cashBalancesId = writeCashBalancesFile_(snapshot);
    }
    file << snapshot.name << "," << toString(snapshot.baseCurrency) << "," << timestampMsString << "," << portfolioID
         << "," << positionsId << "," << cashBalancesId << "\n";
}

std::string CSVPortfolioRepository::writeSnapshotPositionsFile_(const PortfolioSnapshot& snapshot) {
    auto path = csvSnapshotPositionsPath_(snapshot.portfolioId, snapshot.timestampMs);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open attributes file for writing: " + path.string());
    }
    file << "# " + snapshot.portfolioId + "_" + std::to_string(snapshot.timestampMs) + ".pos\n";
    for (const SnapshotPosition& pos : snapshot.positions) {
        file << assetTypeToString(pos.assetId.type) << ":" << pos.assetId.ticker << "," << std::to_string(pos.quantity)
             << "\n";
    }
    return path.stem();
}

std::string CSVPortfolioRepository::writeCashBalancesFile_(const PortfolioSnapshot& snapshot) {
    auto path = csvSnapshotCashBalancesPath_(snapshot.portfolioId, snapshot.timestampMs);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open attributes file for writing: " + path.string());
    }
    file << "# " + snapshot.portfolioId + "_" + std::to_string(snapshot.timestampMs) + ".cash\n";
    for (const auto& [currency, quantity] : snapshot.cashBalances) {
        file << toString(currency) << "=" << std::to_string(quantity) << "\n";
    }
    return path.stem();
}

void CSVPortfolioRepository::writeFullTransactionsCsv_(const std::filesystem::path& path,
                                                       const std::vector<Transaction>& transactions) {
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open CSV File of Transactions for writing: " + path.string());
    }
    file << "timestampMs,type,asset_type,asset_ticker,quantity,price_per_unit,fees,settlement_currency\n";
    for (const Transaction& transaction : transactions) {
        file << std::to_string(transaction.timestampsMs) << "," << toString(transaction.type) << ","
             << assetTypeToString(transaction.assetType) << "," << transaction.assetTicker << ","
             << std::to_string(transaction.quantity) << "," << std::to_string(transaction.pricePerUnit) << ","
             << std::to_string(transaction.fees) << "," << toString(transaction.settlementCurrency) << "\n";
    }
}

void CSVPortfolioRepository::appendTransactionsCsv_(const std::filesystem::path& path,
                                                    const std::vector<Transaction>& transactions) {
    std::ofstream file(path, std::ios::app);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open CSV File of Transactions for writing: " + path.string());
    }
    for (const Transaction& transaction : transactions) {
        file << std::to_string(transaction.timestampsMs) << "," << toString(transaction.type) << ","
             << assetTypeToString(transaction.assetType) << "," << transaction.assetTicker << ","
             << std::to_string(transaction.quantity) << "," << std::to_string(transaction.pricePerUnit) << ","
             << std::to_string(transaction.fees) << "," << toString(transaction.settlementCurrency) << "\n";
    }
}
std::optional<PortfolioSnapshot> CSVPortfolioRepository::parseSnapshotCsvFile_(
    const std::filesystem::path& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open the file to parse the snapshot: " + path.string());
    }
    std::optional<PortfolioSnapshot> output;
    std::string line, prevLine;
    std::getline(file, line);
    prevLine = std::move(line);
    std::getline(file, line);
    prevLine = std::move(line);
    while (getline(file, line)) {
        prevLine = std::move(line);
    }
    if (prevLine.empty()) {
        return output;
    }
    std::istringstream cell(prevLine);
    std::string name, baseCurrencyString, timestampsMsString, portfolioId, positionsId, cashBalancesId;
    if (std::getline(cell, name, ',') && std::getline(cell, baseCurrencyString, ',') &&
        std::getline(cell, timestampsMsString, ',') && std::getline(cell, portfolioId, ',') &&
        std::getline(cell, positionsId, ',')) {
        std::getline(cell, cashBalancesId);  // optional — may be empty at EOF
        std::unordered_map<Currency, double> cashBalance;
        std::vector<SnapshotPosition> positionsSnapshot;
        if (!positionsId.empty()) {
            auto positionsPath = positionFilePath_(positionsId);
            positionsSnapshot = std::move(parsePositionsSnapshotFile_(positionsPath));
        }
        if (!cashBalancesId.empty()) {
            auto cashbalancePath = cashBalanceFilePath_(cashBalancesId);
            cashBalance = std::move(parseCashBalanceFile_(cashbalancePath));
        }
        output = PortfolioSnapshot{name,
                                   currencyFromString(baseCurrencyString),
                                   std::stoll(timestampsMsString),
                                   portfolioId,
                                   positionsSnapshot,
                                   cashBalance};
    }
    return output;
}

std::vector<SnapshotPosition> CSVPortfolioRepository::parsePositionsSnapshotFile_(
    const std::filesystem::path& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open the file to parse the snapshot: " + path.string());
    }
    std::string line;
    std::string assetTypeString, assetTicker, assetQuantityString;
    std::vector<SnapshotPosition> output;
    if (std::getline(file, line)) {
        if (!line.empty() && !std::isalpha(static_cast<unsigned char>(line[0])) && line[0] != '#') {
            std::istringstream iss(line);
            if (std::getline(iss, assetTypeString, ':') && std::getline(iss, assetTicker, ',') &&
                std::getline(iss, assetQuantityString, '\n')) {
                output.push_back(SnapshotPosition{AssetId{assetTypeFromString(assetTypeString), assetTicker},
                                                  std::stod(assetQuantityString)});
            }
        }
    }
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        if (std::getline(iss, assetTypeString, ':') && std::getline(iss, assetTicker, ',') &&
            std::getline(iss, assetQuantityString, '\n')) {
            output.push_back(SnapshotPosition{AssetId{assetTypeFromString(assetTypeString), assetTicker},
                                              std::stod(assetQuantityString)});
        }
    }
    return output;
}

std::unordered_map<Currency, double> CSVPortfolioRepository::parseCashBalanceFile_(
    const std::filesystem::path& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open the file to parse the cash Balance: " + path.string());
    }
    std::string line;
    std::string currencyString, currencyQuantityString;
    std::unordered_map<Currency, double> output;
    if (std::getline(file, line)) {
        if (!line.empty() && !std::isalpha(static_cast<unsigned char>(line[0])) && line[0] != '#') {
            std::istringstream iss(line);
            if (std::getline(iss, currencyString, '=') && std::getline(iss, currencyQuantityString, '\n')) {
                output[currencyFromString(currencyString)] = std::stod(currencyQuantityString);
            }
        }
    }
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        if (std::getline(iss, currencyString, '=') && std::getline(iss, currencyQuantityString, '\n')) {
            output[currencyFromString(currencyString)] = std::stod(currencyQuantityString);
        }
    }
    return output;
}
std::vector<Transaction> CSVPortfolioRepository::parseTransactionsCsvFile_(const std::filesystem::path& path,
                                                                           int64_t afterTimestamps) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open the file to parse the Transactions: " + path.string());
    }
    std::vector<Transaction> output;
    output.reserve(200);
    std::string line;
    std::string timeStampsMsString, transactionTypeString, assetTypeString, assetTicker, quantityString,
        pricePerUnitString, feesString, settlementCurrencystring;
    int64_t timestampMs;
    if (std::getline(file, line)) {
        if (!line.empty() && !std::isalpha(static_cast<unsigned char>(line[0])) && line[0] != '#') {
            std::istringstream iss(line);
            if (std::getline(iss, timeStampsMsString, ',')) {
                timestampMs = std::stoll(timeStampsMsString);
                if (timestampMs >= afterTimestamps) {
                    if (std::getline(iss, transactionTypeString, ',') && std::getline(iss, assetTypeString, ',') &&
                        std::getline(iss, assetTicker, ',') && std::getline(iss, quantityString, ',') &&
                        std::getline(iss, pricePerUnitString, ',') && std::getline(iss, feesString, ',') &&
                        std::getline(iss, settlementCurrencystring, '\n')) {
                        output.push_back(Transaction{timestampMs, transactionTypeFromString(transactionTypeString),
                                                     assetTypeFromString(assetTypeString), assetTicker,
                                                     std::stod(quantityString), std::stod(pricePerUnitString),
                                                     std::stod(feesString),
                                                     currencyFromString(settlementCurrencystring)});
                    }
                }
            }
        }
    }
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        if (std::getline(iss, timeStampsMsString, ',')) {
            timestampMs = std::stoll(timeStampsMsString);
            if (timestampMs < afterTimestamps) continue;
            if (std::getline(iss, transactionTypeString, ',') && std::getline(iss, assetTypeString, ',') &&
                std::getline(iss, assetTicker, ',') && std::getline(iss, quantityString, ',') &&
                std::getline(iss, pricePerUnitString, ',') && std::getline(iss, feesString, ',') &&
                std::getline(iss, settlementCurrencystring, '\n')) {
                output.push_back(Transaction{timestampMs, transactionTypeFromString(transactionTypeString),
                                             assetTypeFromString(assetTypeString), assetTicker,
                                             std::stod(quantityString), std::stod(pricePerUnitString),
                                             std::stod(feesString), currencyFromString(settlementCurrencystring)});
            }
        }
    }
    return output;
}
}  // namespace finapp
