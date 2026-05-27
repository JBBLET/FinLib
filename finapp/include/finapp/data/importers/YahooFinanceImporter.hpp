// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "finapp/finance/common/Currency.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"

namespace finapp {

// Parses a Yahoo Finance portfolio CSV export into a list of Transactions.
//
// Usage:
//   auto txs = YahooFinanceImporter::parse(path, {Currency::EUR});
//   portfolioService.createNew(id, name, Currency::EUR, 0);
//   portfolioRepo.appendTransactions(id, txs);
//
// The importer is stateless and has no repository or network dependency.
// All non-cash symbols are mapped to AssetType::Equity (Phase 1).
//
// Optional column 17 — "Currency":
//   Append a Currency column after Transaction Type to set the settlement currency
//   per row.  When present and a recognised currency code, it overrides both the
//   currencyResolver (for equity rows) and baseCurrency (for $$CASH_TX rows).
//   Rows that omit the column or leave it blank continue to use the existing
//   fallback logic, so existing CSVs without the column remain valid.
//
//   Example extended header:
//     ...,Transaction Type,Currency
//   Example rows:
//     $$CASH_TX,...,DEPOSIT,JPY        ← cash deposit in JPY
//     AMZN,...,BUY,USD                 ← buy settles in USD
//     AIR.PA,...,BUY,EUR               ← buy settles in EUR
//
//   This is the recommended way to handle multi-currency portfolios (e.g. a
//   JPY-base NISA account holding USD-denominated ETFs): without it, all equity
//   prices are deducted as if they were in the portfolio's base currency.
class YahooFinanceImporter {
 public:
    struct Config {
        // Used as the settlement currency for $$CASH_TX rows, and as the
        // fallback settlement currency for equity rows when no resolver is set
        // and no per-row Currency column is present.
        finance::Currency baseCurrency;

        // Optional: called for each non-cash ticker to determine the currency
        // in which the trade settles (i.e. what cash balance is debited on a
        // BUY). If nullptr, baseCurrency is used for every equity row.
        // Per-row column 17 takes precedence over this resolver when set.
        //
        // Example — resolve via yfinance metadata:
        //   config.currencyResolver = [&](const std::string& ticker) {
        //       return equityProvider.fetch(ticker)->denomination();
        //   };
        std::function<finance::Currency(const std::string& ticker)> currencyResolver;
    };

    // Parses the CSV and returns transactions sorted chronologically.
    // Skips rows with unrecognised transaction types (e.g. SPLIT — no price
    // data in the Yahoo export).
    // Throws std::runtime_error if the file cannot be opened.
    static std::vector<finance::Transaction> parse(const std::filesystem::path& csvPath, const Config& config);

    // Same as parse() but reads from an in-memory CSV string (e.g. data received over gRPC).
    static std::vector<finance::Transaction> parseFromString(const std::string& csvData, const Config& config);

 private:
    static std::vector<finance::Transaction> parseStream_(std::istream& stream, const Config& config);
    static int64_t yyyymmddToMs_(const std::string& yyyymmdd);
    static double parseOptionalDouble_(const std::string& s);
};

}  // namespace finapp
