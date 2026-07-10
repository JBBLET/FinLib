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
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "csv/convert.hpp"
#include "csv/csvReader.hpp"
#include "csv/csvReaderAware.hpp"
#include "csv/csvWriter.hpp"
#include "csv/csvWriterAware.hpp"
#include "finapp/common/Exception.hpp"
#include "finapp/common/logger/PrefixedLogger.hpp"
#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/common/AssetId.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finapp/finance/portfolio/PortfolioSnapshot.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"

using finance::AssetId;
using finance::assetTypeFromString;
using finance::Currency;
using finance::currencyFromString;
using finance::PortfolioSnapshot;
using finance::SnapshotPosition;
using finance::Transaction;
using finance::transactionTypeFromString;
namespace finapp {

namespace {
// Column order for the ';'-separated transaction log and snapshot index.
const std::vector<std::string> kTransactionHeader = {"id",
                                                     "timestampMs",
                                                     "type",
                                                     "asset_type",
                                                     "asset_ticker",
                                                     "quantity",
                                                     "price_per_unit",
                                                     "fees",
                                                     "settlement_currency"};
const std::vector<std::string> kSnapshotHeader = {
    "name", "baseCurrency", "timestampMs", "portfolioID", "positions_id", "cashBalances_id"};

Row transactionToRow(const Transaction& t) {
    return Row{{"id", t.id},
               {"timestampMs", std::to_string(t.timestampsMs)},
               {"type", toString(t.type)},
               {"asset_type", assetTypeToString(t.assetType)},
               {"asset_ticker", t.assetTicker},
               {"quantity", std::to_string(t.quantity)},
               {"price_per_unit", std::to_string(t.pricePerUnit)},
               {"fees", std::to_string(t.fees)},
               {"settlement_currency", toString(t.settlementCurrency)}};
}

// Serializes a transaction as an ordered record matching kTransactionHeader.
std::vector<std::string> transactionToRecord(const Transaction& t) {
    Row row = transactionToRow(t);
    std::vector<std::string> record;
    record.reserve(kTransactionHeader.size());
    for (const auto& col : kTransactionHeader) record.push_back(row.at(col));
    return record;
}

Transaction transactionFromRow(const Row& r) {
    return Transaction{r.at("id"),
                       csv::convert::parseInt<int64_t>(r.at("timestampMs")),
                       transactionTypeFromString(r.at("type")),
                       assetTypeFromString(r.at("asset_type")),
                       r.at("asset_ticker"),
                       csv::convert::parseFloat<double>(r.at("quantity")),
                       csv::convert::parseFloat<double>(r.at("price_per_unit")),
                       csv::convert::parseFloat<double>(r.at("fees")),
                       currencyFromString(r.at("settlement_currency"))};
}
}  // namespace

CSVPortfolioRepository::CSVPortfolioRepository(std::filesystem::path directory, finapp::logging::ILogger* logger)
    : directory_{std::move(directory)},
      logger_{finapp::logging::PrefixedLogger::wrap(logger, "CSVPortfolioRepository")} {
    std::filesystem::create_directories(directory_ / "Portfolio" / "Positions");
    std::filesystem::create_directories(directory_ / "Portfolio" / "Cash");
    if (logger_) logger_->write(finapp::logging::Level::Info, "directory: " + directory_.string());
}

// IPortfolioRepository Interface
void CSVPortfolioRepository::saveSnapshot(const PortfolioSnapshot& snapshot) {
    if (logger_)
        logger_->write(finapp::logging::Level::Debug,
                       "saveSnapshot '" + snapshot.portfolioId + "' t=" + std::to_string(snapshot.timestampMs));
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

std::optional<PortfolioSnapshot> CSVPortfolioRepository::loadClosestSnapshot(const std::string& portfolioId,
                                                                             const Timestamp& ts) const {
    auto path = csvSnapshotPath_(portfolioId);
    if (!std::filesystem::exists(path)) return std::nullopt;
    auto all = parseAllSnapshotRows_(path);
    if (all.empty()) return std::nullopt;
    std::sort(all.begin(), all.end(), [](const PortfolioSnapshot& a, const PortfolioSnapshot& b) {
        return a.timestampMs < b.timestampMs;
    });
    auto upper = std::upper_bound(
        all.begin(), all.end(), ts, [](Timestamp t, const PortfolioSnapshot& s) { return t < s.timestampMs; });
    if (upper == all.begin()) return std::nullopt;
    return *std::prev(upper);
}

void CSVPortfolioRepository::appendTransactions(const std::string& portfolioID,
                                                const std::vector<Transaction>& transactions) {
    if (logger_)
        logger_->write(finapp::logging::Level::Debug,
                       "appendTransactions '" + portfolioID + "' count=" + std::to_string(transactions.size()));
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
    if (logger_)
        logger_->write(finapp::logging::Level::Debug,
                       "loadTransactions '" + portfolioId + "' after=" + std::to_string(afterTimestamps));
    auto path = csvTransactionsPath_(portfolioId);
    if (!std::filesystem::exists(path)) {
        if (!exists(portfolioId))
            throw finapp::Exception("CSVPortfolioRepository::loadTransactions: portfolio '" + portfolioId +
                                    "' not found");
        return {};
    }
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
    if (logger_)
        logger_->write(finapp::logging::Level::Info,
                       "deleteTransaction '" + transactionId + "' from '" + portfolioId + "'");
    const auto transactionsPath = csvTransactionsPath_(portfolioId);
    if (!std::filesystem::exists(transactionsPath)) {
        throw finapp::Exception("CSVPortfolioRepository::deleteTransaction: no transactions file for portfolio '" +
                                portfolioId + "'.");
    }

    // Find the transaction by id, record its timestamp for snapshot invalidation, then rewrite the log.
    std::vector<Transaction> all = parseTransactionsCsvFile_(transactionsPath, 0);
    auto it = std::find_if(all.begin(), all.end(), [&](const Transaction& t) { return t.id == transactionId; });
    if (it == all.end()) {
        throw finapp::Exception("CSVPortfolioRepository::deleteTransaction: transaction '" + transactionId +
                                "' not found in portfolio '" + portfolioId + "'.");
    }
    const int64_t deletedTimestampMs = it->timestampsMs;
    all.erase(it);
    writeFullTransactionsCsv_(transactionsPath, all);

    // Invalidate every snapshot at or after the deleted transaction's timestamp.
    trimSnapshotRowsFrom_(portfolioId, deletedTimestampMs);
}

void CSVPortfolioRepository::deletePortfolio(const std::string& portfolioId) {
    if (logger_) logger_->write(finapp::logging::Level::Info, "deletePortfolio '" + portfolioId + "'");
    const auto snapshotPath = csvSnapshotPath_(portfolioId);
    const auto transactionsPath = csvTransactionsPath_(portfolioId);

    if (!std::filesystem::exists(snapshotPath) && !std::filesystem::exists(transactionsPath)) {
        throw finapp::Exception("CSVPortfolioRepository::deletePortfolio: portfolio '" + portfolioId +
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
    if (logger_)
        logger_->write(finapp::logging::Level::Debug,
                       "replaceSnapshotsFrom '" + portfolioId + "' from=" + std::to_string(fromTimestampMs) +
                           " newCount=" + std::to_string(newSnapshots.size()));
    const auto snapshotPath = csvSnapshotPath_(portfolioId);
    std::filesystem::create_directories(snapshotPath.parent_path());

    // Single read pass: keep rows before the cut-off, delete .pos/.cash of trimmed rows.
    std::vector<Row> keptRows;
    if (std::filesystem::exists(snapshotPath) && std::filesystem::file_size(snapshotPath) > 0) {
        std::ifstream inFile(snapshotPath);
        if (!inFile.is_open())
            throw finapp::Exception("Cannot open snapshot CSV for reading: " + snapshotPath.string());
        CSVReaderHeaderAware reader{CSVReader{inFile, ';'}};
        for (auto& row : reader.readAllMaps()) {
            if (csv::convert::parseInt<int64_t>(row.at("timestampMs")) >= fromTimestampMs) {
                const auto& positionsId = row.at("positions_id");
                const auto& cashBalancesId = row.at("cashBalances_id");
                if (!positionsId.empty()) {
                    auto p = positionFilePath_(positionsId);
                    if (std::filesystem::exists(p)) std::filesystem::remove(p);
                }
                if (!cashBalancesId.empty()) {
                    auto p = cashBalanceFilePath_(cashBalancesId);
                    if (std::filesystem::exists(p)) std::filesystem::remove(p);
                }
            } else {
                keptRows.push_back(std::move(row));
            }
        }
    }

    // Write all .pos/.cash files for new snapshots and build their CSV rows.
    std::vector<Row> newRows;
    newRows.reserve(newSnapshots.size());
    for (const auto& snap : newSnapshots) {
        std::string positionsId;
        std::string cashBalancesId;
        if (!snap.positions.empty()) positionsId = writeSnapshotPositionsFile_(snap);
        if (!snap.cashBalances.empty()) cashBalancesId = writeCashBalancesFile_(snap);
        newRows.push_back(Row{{"name", snap.name},
                              {"baseCurrency", toString(snap.baseCurrency)},
                              {"timestampMs", std::to_string(snap.timestampMs)},
                              {"portfolioID", snap.portfolioId},
                              {"positions_id", positionsId},
                              {"cashBalances_id", cashBalancesId}});
    }

    // Single write pass: header + surviving rows + new rows.
    std::ofstream outFile(snapshotPath, std::ios::trunc);
    if (!outFile.is_open()) throw finapp::Exception("Cannot open snapshot CSV for writing: " + snapshotPath.string());
    CSVWriterHeaderAware writer{CSVWriter{outFile, ';'}, kSnapshotHeader};
    writer.writeAllMaps(keptRows, false);
    writer.writeAllMaps(newRows, false);
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
        throw finapp::Exception("Cannot open attributes file for writing: " + path.string());
    }
    file << "# " + snapshot.portfolioId + "_" + std::to_string(snapshot.timestampMs) + ".pos\n";
    for (const SnapshotPosition& pos : snapshot.positions) {
        file << assetTypeToString(pos.assetId.type) << ":" << pos.assetId.ticker << ";" << std::to_string(pos.quantity)
             << "\n";
    }
    return path.stem();
}

std::string CSVPortfolioRepository::writeCashBalancesFile_(const PortfolioSnapshot& snapshot) {
    auto path = csvSnapshotCashBalancesPath_(snapshot.portfolioId, snapshot.timestampMs);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw finapp::Exception("Cannot open attributes file for writing: " + path.string());
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
        throw finapp::Exception("Cannot open CSV File of Transactions for writing: " + path.string());
    }
    CSVWriterHeaderAware writer{CSVWriter{file, ';'}, kTransactionHeader};
    for (const Transaction& transaction : transactions) {
        writer.writeMap(transactionToRow(transaction), false);
    }
}

void CSVPortfolioRepository::appendTransactionsCsv_(const std::filesystem::path& path,
                                                    const std::vector<Transaction>& transactions) {
    std::ofstream file(path, std::ios::app);
    if (!file.is_open()) {
        throw finapp::Exception("Cannot open CSV File of Transactions for writing: " + path.string());
    }
    // Append only — the header already exists, so use the low-level writer without re-emitting it.
    CSVWriter writer{file, ';'};
    for (const Transaction& transaction : transactions) {
        writer.writeNext(transactionToRecord(transaction), false);
    }
}
PortfolioSnapshot CSVPortfolioRepository::snapshotFromFields_(const std::string& name, const std::string& baseCurrency,
                                                             const std::string& timestampMs,
                                                             const std::string& portfolioId,
                                                             const std::string& positionsId,
                                                             const std::string& cashBalancesId) const {
    std::vector<SnapshotPosition> positions;
    std::unordered_map<Currency, double> cashBalance;
    if (!positionsId.empty()) {
        positions = parsePositionsSnapshotFile_(positionFilePath_(positionsId));
    }
    if (!cashBalancesId.empty()) {
        cashBalance = parseCashBalanceFile_(cashBalanceFilePath_(cashBalancesId));
    }
    return PortfolioSnapshot{name,
                             currencyFromString(baseCurrency),
                             csv::convert::parseInt<int64_t>(timestampMs),
                             portfolioId,
                             std::move(positions),
                             std::move(cashBalance)};
}

std::optional<PortfolioSnapshot> CSVPortfolioRepository::parseSnapshotCsvFile_(
    const std::filesystem::path& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw finapp::Exception("Could not open the file to parse the snapshot: " + path.string());
    }
    CSVReaderHeaderAware reader{CSVReader{file, ';'}};
    auto rows = reader.readAllMaps();
    if (rows.empty()) return std::nullopt;
    const Row& r = rows.back();
    return snapshotFromFields_(r.at("name"),
                               r.at("baseCurrency"),
                               r.at("timestampMs"),
                               r.at("portfolioID"),
                               r.at("positions_id"),
                               r.at("cashBalances_id"));
}

std::vector<PortfolioSnapshot> CSVPortfolioRepository::parseAllSnapshotRows_(const std::filesystem::path& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw finapp::Exception("Could not open snapshot file: " + path.string());
    }
    CSVReaderHeaderAware reader{CSVReader{file, ';'}};
    std::vector<PortfolioSnapshot> output;
    for (const auto& r : reader.readAllMaps()) {
        output.push_back(snapshotFromFields_(r.at("name"),
                                             r.at("baseCurrency"),
                                             r.at("timestampMs"),
                                             r.at("portfolioID"),
                                             r.at("positions_id"),
                                             r.at("cashBalances_id")));
    }
    return output;
}

std::vector<SnapshotPosition> CSVPortfolioRepository::parsePositionsSnapshotFile_(
    const std::filesystem::path& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw finapp::Exception("Could not open the file to parse the snapshot: " + path.string());
    }
    std::string line;
    std::string assetTypeString, assetTicker, assetQuantityString;
    std::vector<SnapshotPosition> output;
    if (std::getline(file, line)) {
        if (!line.empty() && !std::isalpha(static_cast<unsigned char>(line[0])) && line[0] != '#') {
            std::istringstream iss(line);
            if (std::getline(iss, assetTypeString, ':') && std::getline(iss, assetTicker, ';') &&
                std::getline(iss, assetQuantityString, '\n')) {
                output.push_back(SnapshotPosition{AssetId{assetTypeFromString(assetTypeString), assetTicker},
                                                  std::stod(assetQuantityString)});
            }
        }
    }
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        if (std::getline(iss, assetTypeString, ':') && std::getline(iss, assetTicker, ';') &&
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
        throw finapp::Exception("Could not open the file to parse the cash Balance: " + path.string());
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
        throw finapp::Exception("Could not open the file to parse the Transactions: " + path.string());
    }
    CSVReaderHeaderAware reader{CSVReader{file, ';'}};
    std::vector<Transaction> output;
    for (const auto& row : reader.readAllMaps()) {
        if (csv::convert::parseInt<int64_t>(row.at("timestampMs")) < afterTimestamps) continue;
        output.push_back(transactionFromRow(row));
    }
    return output;
}
}  // namespace finapp
