// Copyright (c) 2026 JBBLET. All Rights Reserved.

#include "finapp/data/repository/implementation/CsvRepository/CSVPortfolioRepository.hpp"

#include <algorithm>
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

CSVPortfolioRepository::CSVPortfolioRepository(std::filesystem::path directory) : directory_{std::move(directory)} {
    std::filesystem::create_directories(directory_ / "Portfolio" / "Positions");
    std::filesystem::create_directories(directory_ / "Portfolio" / "Cash");
}

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

std::vector<PortfolioSnapshot> CSVPortfolioRepository::loadAllSnapshots(const std::string& portfolioId) const {
    auto path = csvSnapshotPath_(portfolioId);
    if (!std::filesystem::exists(path)) return {};
    return parseAllSnapshotRows_(path);
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
            transactions.begin(),
            transactions.end(),
            std::back_inserter(transactionsNotSaved),
            [&](const Transaction& transaction) { return existingSet.find(transaction) == existingSet.end(); });
        appendTransactionsCsv_(path, transactionsNotSaved);
    }
}

std::vector<Transaction> CSVPortfolioRepository::loadTransactions(const std::string& portfolioId,
                                                                  int64_t afterTimestamps) const {
    auto path = csvTransactionsPath_(portfolioId);
    if (!std::filesystem::exists(path)) return {};
    return parseTransactionsCsvFile_(path, afterTimestamps);
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

void CSVPortfolioRepository::deleteTransaction(const std::string& portfolioId, const std::string& transactionId) {
    const auto transactionsPath = csvTransactionsPath_(portfolioId);
    if (!std::filesystem::exists(transactionsPath)) {
        throw std::runtime_error("CSVPortfolioRepository::deleteTransaction: no transactions file for portfolio '" +
                                 portfolioId + "'.");
    }

    // Find the transaction by id, record its timestamp for snapshot invalidation, then rewrite the log.
    std::vector<Transaction> all = parseTransactionsCsvFile_(transactionsPath, 0);
    auto it = std::find_if(all.begin(), all.end(), [&](const Transaction& t) { return t.id == transactionId; });
    if (it == all.end()) {
        throw std::runtime_error("CSVPortfolioRepository::deleteTransaction: transaction '" + transactionId +
                                 "' not found in portfolio '" + portfolioId + "'.");
    }
    const int64_t deletedTimestampMs = it->timestampsMs;
    all.erase(it);
    writeFullTransactionsCsv_(transactionsPath, all);

    // Invalidate every snapshot at or after the deleted transaction's timestamp.
    trimSnapshotRowsFrom_(portfolioId, deletedTimestampMs);
}

void CSVPortfolioRepository::deletePortfolio(const std::string& portfolioId) {
    const auto snapshotPath = csvSnapshotPath_(portfolioId);
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

void CSVPortfolioRepository::replaceSnapshotsFrom(const std::string& portfolioId, int64_t fromTimestampMs,
                                                  const std::vector<PortfolioSnapshot>& newSnapshots) {
    const auto snapshotPath = csvSnapshotPath_(portfolioId);
    std::filesystem::create_directories(snapshotPath.parent_path());

    // Single read pass: keep rows before the cut-off, delete .pos/.cash of trimmed rows.
    std::vector<std::string> keptRows;
    if (std::filesystem::exists(snapshotPath) && std::filesystem::file_size(snapshotPath) > 0) {
        std::ifstream inFile(snapshotPath);
        if (!inFile.is_open())
            throw std::runtime_error("Cannot open snapshot CSV for reading: " + snapshotPath.string());
        std::string line;
        std::getline(inFile, line);  // skip header
        while (std::getline(inFile, line)) {
            if (line.empty()) continue;
            std::istringstream iss(line);
            std::string name, baseCurrency, timestampMsStr, portId, positionsId, cashBalancesId;
            if (!std::getline(iss, name, ',') || !std::getline(iss, baseCurrency, ',') ||
                !std::getline(iss, timestampMsStr, ',') || !std::getline(iss, portId, ',') ||
                !std::getline(iss, positionsId, ','))
                continue;
            std::getline(iss, cashBalancesId);
            if (std::stoll(timestampMsStr) >= fromTimestampMs) {
                if (!positionsId.empty()) {
                    auto p = positionFilePath_(positionsId);
                    if (std::filesystem::exists(p)) std::filesystem::remove(p);
                }
                if (!cashBalancesId.empty()) {
                    auto p = cashBalanceFilePath_(cashBalancesId);
                    if (std::filesystem::exists(p)) std::filesystem::remove(p);
                }
            } else {
                keptRows.push_back(line);
            }
        }
    }

    // Write all .pos/.cash files for new snapshots and build their CSV rows.
    std::vector<std::string> newRows;
    newRows.reserve(newSnapshots.size());
    for (const auto& snap : newSnapshots) {
        std::string positionsId;
        std::string cashBalancesId;
        if (!snap.positions.empty()) positionsId = writeSnapshotPositionsFile_(snap);
        if (!snap.cashBalances.empty()) cashBalancesId = writeCashBalancesFile_(snap);
        newRows.push_back(snap.name + "," + toString(snap.baseCurrency) + "," + std::to_string(snap.timestampMs) + "," +
                          snap.portfolioId + "," + positionsId + "," + cashBalancesId);
    }

    // Single write pass: header + surviving rows + new rows.
    std::ofstream outFile(snapshotPath, std::ios::trunc);
    if (!outFile.is_open()) throw std::runtime_error("Cannot open snapshot CSV for writing: " + snapshotPath.string());
    outFile << "name,baseCurrency,timestampMs,portfolioID,positions_id,cashBalances_id\n";
    for (const auto& row : keptRows) outFile << row << '\n';
    for (const auto& row : newRows) outFile << row << '\n';
}

void CSVPortfolioRepository::trimSnapshotRowsFrom_(const std::string& portfolioId, int64_t fromTimestampMs) {
    replaceSnapshotsFrom(portfolioId, fromTimestampMs, {});
}

void CSVPortfolioRepository::writeSnapshotCsv_(const std::string& portfolioID, const PortfolioSnapshot& snapshot) {
    replaceSnapshotsFrom(portfolioID, snapshot.timestampMs, {snapshot});
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
    file << "id,timestampMs,type,asset_type,asset_ticker,quantity,price_per_unit,fees,settlement_currency\n";
    for (const Transaction& transaction : transactions) {
        file << transaction.id << "," << std::to_string(transaction.timestampsMs) << "," << toString(transaction.type)
             << "," << assetTypeToString(transaction.assetType) << "," << transaction.assetTicker << ","
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
        file << transaction.id << "," << std::to_string(transaction.timestampsMs) << "," << toString(transaction.type)
             << "," << assetTypeToString(transaction.assetType) << "," << transaction.assetTicker << ","
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

std::vector<PortfolioSnapshot> CSVPortfolioRepository::parseAllSnapshotRows_(const std::filesystem::path& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open snapshot file: " + path.string());
    }
    std::vector<PortfolioSnapshot> output;
    std::string line;
    std::getline(file, line);  // skip header
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream cell(line);
        std::string name, baseCurrencyString, timestampsMsString, portfolioId, positionsId, cashBalancesId;
        if (!std::getline(cell, name, ',') || !std::getline(cell, baseCurrencyString, ',') ||
            !std::getline(cell, timestampsMsString, ',') || !std::getline(cell, portfolioId, ',') ||
            !std::getline(cell, positionsId, ','))
            continue;
        std::getline(cell, cashBalancesId);
        std::vector<SnapshotPosition> positions;
        std::unordered_map<Currency, double> cashBalance;
        if (!positionsId.empty()) {
            positions = parsePositionsSnapshotFile_(positionFilePath_(positionsId));
        }
        if (!cashBalancesId.empty()) {
            cashBalance = parseCashBalanceFile_(cashBalanceFilePath_(cashBalancesId));
        }
        output.push_back(PortfolioSnapshot{name,
                                           currencyFromString(baseCurrencyString),
                                           std::stoll(timestampsMsString),
                                           portfolioId,
                                           positions,
                                           cashBalance});
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
    std::string idString, timeStampsMsString, transactionTypeString, assetTypeString, assetTicker, quantityString,
        pricePerUnitString, feesString, settlementCurrencystring;
    int64_t timestampMs;

    auto parseLine = [&](const std::string& l) -> bool {
        std::istringstream iss(l);
        if (!std::getline(iss, idString, ',') || !std::getline(iss, timeStampsMsString, ',')) return false;
        timestampMs = std::stoll(timeStampsMsString);
        if (timestampMs < afterTimestamps) return false;
        if (!std::getline(iss, transactionTypeString, ',') || !std::getline(iss, assetTypeString, ',') ||
            !std::getline(iss, assetTicker, ',') || !std::getline(iss, quantityString, ',') ||
            !std::getline(iss, pricePerUnitString, ',') || !std::getline(iss, feesString, ',') ||
            !std::getline(iss, settlementCurrencystring))
            return false;
        output.push_back(Transaction{idString,
                                     timestampMs,
                                     transactionTypeFromString(transactionTypeString),
                                     assetTypeFromString(assetTypeString),
                                     assetTicker,
                                     std::stod(quantityString),
                                     std::stod(pricePerUnitString),
                                     std::stod(feesString),
                                     currencyFromString(settlementCurrencystring)});
        return true;
    };

    // Skip header; handle files that have no header (first char is a digit).
    if (std::getline(file, line)) {
        if (!line.empty() && (std::isdigit(static_cast<unsigned char>(line[0])) || line[0] == '#')) {
            parseLine(line);
        }
    }
    while (std::getline(file, line)) {
        if (!line.empty()) parseLine(line);
    }
    return output;
}
}  // namespace finapp
