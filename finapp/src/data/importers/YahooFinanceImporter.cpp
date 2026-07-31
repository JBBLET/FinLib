// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finapp/data/importers/YahooFinanceImporter.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "cpputils/file.hpp"
#include "csv/convert.hpp"
#include "csv/csvReader.hpp"
#include "csv/csvReaderAware.hpp"
#include "finapp/common/Error.hpp"
#include "finapp/common/Log.hpp"
#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"
#include "finlib/common/utils/TimeUtils.hpp"

namespace finapp {

namespace {
// Yahoo Finance CSV column names.
constexpr const char* kColSymbol = "Symbol";
constexpr const char* kColTradeDate = "Trade Date";
constexpr const char* kColPurchasePrice = "Purchase Price";
constexpr const char* kColQuantity = "Quantity";
constexpr const char* kColCommission = "Commission";
constexpr const char* kColTxType = "Transaction Type";
// Optional column. When present and non-empty, its value overrides the settlement currency for
// that row (both equity buys/sells and $$CASH_TX deposits/withdrawals).
constexpr const char* kColCurrency = "Currency";
}  // namespace

std::vector<finance::Transaction> YahooFinanceImporter::parse(const std::filesystem::path& csvPath,
                                                              const Config& config) {
    auto file = cpputils::files::File{csvPath}.read();
    logging::info("parse: {}", csvPath.string());
    return parseStream_(file, config);
}

std::vector<finance::Transaction> YahooFinanceImporter::parseFromString(const std::string& csvData,
                                                                        const Config& config) {
    std::istringstream stream(csvData);
    return parseStream_(stream, config);
}

std::vector<finance::Transaction> YahooFinanceImporter::parseStream_(std::istream& stream, const Config& config) {
    auto resolveCurrency = config.currencyResolver ? config.currencyResolver
                                                   : [&config](const std::string&) { return config.baseCurrency; };

    std::vector<finance::Transaction> result;

    cpputils::csv::CSVReaderHeaderAware reader{cpputils::csv::CSVReader{stream}};  // default ',' separator
    const auto& headers = reader.headers();
    const bool hasCurrencyCol = std::ranges::find(headers, kColCurrency) != headers.end();

    for (const auto& row : reader.readAllMaps()) {
        const std::string& symbol = row.at(kColSymbol);
        const std::string& dateStr = row.at(kColTradeDate);
        const std::string& txType = row.at(kColTxType);

        if (dateStr.size() != 8) continue;  // malformed date — skip

        const int64_t timestampMs = yyyymmddToMs_(dateStr);
        const double quantity = parseOptionalDouble_(row.at(kColQuantity));
        const double price = parseOptionalDouble_(row.at(kColPurchasePrice));
        const double fees = parseOptionalDouble_(row.at(kColCommission));

        // Resolve the settlement currency: the Currency column wins when present and valid;
        // otherwise fall back to the resolver (equity) or baseCurrency (cash).
        auto readCurrencyCol = [&]() -> std::optional<finance::Currency> {
            if (hasCurrencyCol) {
                const std::string& c = row.at(kColCurrency);
                if (!c.empty()) {
                    try {
                        return finance::currencyFromString(c);
                    } catch (...) {
                        // Unrecognised string — treat as absent.
                    }
                }
            }
            return std::nullopt;
        };

        if (symbol == "$$CASH_TX") {
            finance::TransactionType type;
            if (txType == "DEPOSIT") {
                type = finance::TransactionType::Deposit;
            } else if (txType == "WITHDRAWAL") {
                type = finance::TransactionType::Withdrawal;
            } else {
                logging::debug("skipped $$CASH_TX row — unknown type '{}'", txType);
                continue;
            }
            const finance::Currency txCurrency = readCurrencyCol().value_or(config.baseCurrency);
            result.push_back(finance::Transaction{"",
                                                  timestampMs,
                                                  type,
                                                  finance::AssetType::Cash,
                                                  toString(txCurrency),
                                                  quantity,
                                                  1.0,
                                                  fees,
                                                  txCurrency});
        } else {
            finance::TransactionType type;
            if (txType == "BUY") {
                type = finance::TransactionType::Buy;
            } else if (txType == "SELL") {
                type = finance::TransactionType::Sell;
            } else if (txType == "DIVIDEND") {
                type = finance::TransactionType::Dividend;
            } else {
                // SPLIT has no price in Yahoo exports — skip and handle manually if needed.
                logging::debug("skipped '{}' row — unsupported type '{}'", symbol, txType);
                continue;
            }
            const finance::Currency settlement = readCurrencyCol().value_or(resolveCurrency(symbol));
            result.push_back(finance::Transaction{
                "", timestampMs, type, finance::AssetType::Equity, symbol, quantity, price, fees, settlement});
        }
    }

    std::sort(result.begin(), result.end(), [](const finance::Transaction& a, const finance::Transaction& b) {
        return a.timestampsMs < b.timestampsMs;
    });
    logging::info("parsed {} transactions", result.size());
    return result;
}

int64_t YahooFinanceImporter::yyyymmddToMs_(const std::string& s) {
    // "20241125" → "2024-11-25T00:00:00Z" (midnight UTC)
    std::string iso = s.substr(0, 4) + "-" + s.substr(4, 2) + "-" + s.substr(6, 2) + "T00:00:00Z";
    return ts::common::utils::time::parseIso8601ToMs(iso);
}

double YahooFinanceImporter::parseOptionalDouble_(const std::string& s) {
    if (s.empty()) return 0.0;
    try {
        return cpputils::csv::convert::parseFloat<double>(s);
    } catch (...) {
        return 0.0;
    }
}

}  // namespace finapp
