// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finapp/data/importers/YahooFinanceImporter.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"
#include "finlib/common/utils/TimeUtils.hpp"

namespace finapp {

using namespace finance;

namespace {
// Yahoo Finance CSV column indices (0-based)
constexpr int kColSymbol        = 0;
constexpr int kColTradeDate     = 9;
constexpr int kColPurchasePrice = 10;
constexpr int kColQuantity      = 11;
constexpr int kColCommission    = 12;
constexpr int kColTxType        = 16;
constexpr int kMinCols          = 17;
}  // namespace

std::vector<Transaction> YahooFinanceImporter::parse(const std::filesystem::path& csvPath, const Config& config) {
    std::ifstream file(csvPath);
    if (!file.is_open()) {
        throw std::runtime_error("YahooFinanceImporter: cannot open file: " + csvPath.string());
    }

    auto resolveCurrency = config.currencyResolver
                               ? config.currencyResolver
                               : [&config](const std::string&) { return config.baseCurrency; };

    std::vector<Transaction> result;
    std::string line;
    std::getline(file, line);  // skip header

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::vector<std::string> cols;
        cols.reserve(kMinCols);
        std::istringstream iss(line);
        std::string cell;
        while (std::getline(iss, cell, ',')) {
            cols.push_back(std::move(cell));
        }
        // Pad so every index up to kMinCols-1 is safe to access.
        while (static_cast<int>(cols.size()) < kMinCols) {
            cols.emplace_back();
        }

        const std::string& symbol  = cols[kColSymbol];
        const std::string& dateStr = cols[kColTradeDate];
        const std::string& txType  = cols[kColTxType];

        if (dateStr.size() != 8) continue;  // malformed date — skip

        const int64_t timestampMs = yyyymmddToMs_(dateStr);
        const double quantity     = parseOptionalDouble_(cols[kColQuantity]);
        const double price        = parseOptionalDouble_(cols[kColPurchasePrice]);
        const double fees         = parseOptionalDouble_(cols[kColCommission]);

        if (symbol == "$$CASH_TX") {
            TransactionType type;
            if (txType == "DEPOSIT") {
                type = TransactionType::Deposit;
            } else if (txType == "WITHDRAWAL") {
                type = TransactionType::Withdrawal;
            } else {
                continue;
            }
            // Ticker for Cash assets is the 3-letter currency code (AssetService convention).
            result.push_back(Transaction{timestampMs, type, AssetType::Cash,
                                         toString(config.baseCurrency), quantity, 1.0, fees,
                                         config.baseCurrency});
        } else {
            TransactionType type;
            if (txType == "BUY") {
                type = TransactionType::Buy;
            } else if (txType == "SELL") {
                type = TransactionType::Sell;
            } else if (txType == "DIVIDEND") {
                type = TransactionType::Dividend;
            } else {
                // SPLIT has no price in Yahoo exports — skip and handle manually if needed.
                continue;
            }
            const Currency settlement = resolveCurrency(symbol);
            result.push_back(
                Transaction{timestampMs, type, AssetType::Equity, symbol, quantity, price, fees, settlement});
        }
    }

    std::sort(result.begin(), result.end(),
              [](const Transaction& a, const Transaction& b) { return a.timestampsMs < b.timestampsMs; });
    return result;
}

int64_t YahooFinanceImporter::yyyymmddToMs_(const std::string& s) {
    // "20241125" → "2024-11-25T00:00:00Z" (midnight UTC)
    std::string iso =
        s.substr(0, 4) + "-" + s.substr(4, 2) + "-" + s.substr(6, 2) + "T00:00:00Z";
    return common::utils::time::parseIso8601ToMs(iso);
}

double YahooFinanceImporter::parseOptionalDouble_(const std::string& s) {
    if (s.empty()) return 0.0;
    try {
        return std::stod(s);
    } catch (...) {
        return 0.0;
    }
}

}  // namespace finapp
