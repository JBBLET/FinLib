// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>

#include "finapp/data/importers/YahooFinanceImporter.hpp"
#include "finapp/data/providers/implementations/Yfinance/YFinanceEquityProvider.hpp"
#include "finapp/data/providers/implementations/Yfinance/YFinanceProvider.hpp"
#include "finapp/data/repository/implementation/CsvRepository/CSVPortfolioRepository.hpp"
#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finapp/finance/portfolio/PortfolioSnapshot.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"
#include "finapp/service/AssetService.hpp"
#include "finapp/service/FXService.hpp"
#include "finapp/service/PortfolioService.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/implementation/CachedTimeSeriesRepository.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"
#include "support/service_test_fakes.hpp"

using namespace finance;
using namespace finapp;
using namespace finapp::test;

namespace {

constexpr int64_t kWeekMs = 7LL * 86'400'000LL;

// Resources directory — injected by CMake at compile time.
const std::filesystem::path kResourcesDir{FINAPP_TEST_RESOURCES_DIR};

// Owns everything needed to run a live portfolio tracking test: a PortfolioService
// wired to live yfinance and a CSVPortfolioRepository in a temp directory.
// The temp directory is deleted on destruction.
struct LiveBundle {
    std::unique_ptr<PortfolioService> service;
    std::filesystem::path repoDir;

    ~LiveBundle() { std::filesystem::remove_all(repoDir); }
};

LiveBundle makeLiveBundle(const std::string& testName) {
    auto repoDir = std::filesystem::temp_directory_path() / ("finapp_live_" + testName);
    std::filesystem::remove_all(repoDir);
    std::filesystem::create_directories(repoDir);

    // Time-series: live yfinance loader backed by an in-memory cache.
    auto innerRepo = std::make_shared<InMemoryTimeSeriesRepository>();
    auto cachedRepo = std::make_shared<CachedTimeSeriesRepository>(innerRepo);
    auto tsLoader = std::make_shared<YFinanceProvider>("/home/jbblet/user/Documents/Projects/FinLib/.venv/bin/python",
                                                       FINAPP_PYTHON_DIR "/YFinanceFetcher.py");
    auto tsService = std::make_shared<TimeSeriesService>(cachedRepo, tsLoader);

    // Asset: in-memory repo + live yfinance equity provider to resolve denomination.
    auto equityRepo = std::make_shared<InMemoryAssetRepository>();
    auto equityProvider = std::make_shared<YFinanceEquityProvider>();
    std::unordered_map<AssetType, std::shared_ptr<IAssetRepository>> repos = {{AssetType::Equity, equityRepo}};
    std::unordered_map<AssetType, std::shared_ptr<IAssetProvider>> providers = {{AssetType::Equity, equityProvider}};
    auto assetService = std::make_shared<AssetService>(tsService, std::move(repos), std::move(providers));

    // FX: empty repo — both portfolios use a single base currency throughout.
    auto fxRepo = std::make_shared<InMemoryFXRepository>();
    auto fxService = std::make_shared<FXService>(tsService, fxRepo);

    auto portfolioRepo = std::make_shared<CSVPortfolioRepository>(repoDir);
    auto service = std::make_unique<PortfolioService>(portfolioRepo, assetService, fxService);

    return {std::move(service), repoDir};
}

void assertValidValueSeries(const TimeSeries& series, const std::string& label) {
    ASSERT_GT(series.size(), 0u) << label << ": series is empty";

    for (size_t i = 0; i < series.getValues().size(); ++i) {
        EXPECT_TRUE(std::isfinite(series.getValues()[i])) << label << ": non-finite value at index " << i;
        EXPECT_GE(series.getValues()[i], 0.0) << label << ": negative value at index " << i;
    }

    EXPECT_GT(series.getValues().back(), 0.0) << label << ": final portfolio value is zero";
}

}  // namespace

// ============================================================
// PEA — Euronext Paris portfolio (EUR-denominated, no FX)
// ============================================================

TEST(PortfolioTrackingLive, PEA_ValueSeriesFromCSV) {
    auto bundle = makeLiveBundle("pea");

    auto txns = YahooFinanceImporter::parse(kResourcesDir / "portfolio PEA.csv",
                                            YahooFinanceImporter::Config{Currency::EUR, nullptr});
    ASSERT_FALSE(txns.empty()) << "No transactions parsed from PEA CSV";

    // Use the full service layer: createNew seeds the T=0 sentinel snapshot,
    // importTransactions runs rebuildSnapshotsFrom_ and applies Portfolio::apply logic.
    // This is the same path as RequestAddTransactionByCsv in the gRPC handler.
    bundle.service->createNew("pea", "PEA", Currency::EUR);

    // The Yahoo Finance export may omit some brokerage deposits (interest credits,
    // wire transfers not captured in the tracker).  Prepend a seed deposit so the
    // cash balance never goes negative when replaying historical buys.
    Transaction seed{};
    seed.timestampsMs = txns.front().timestampsMs - 1;
    seed.type = TransactionType::Deposit;
    seed.assetType = AssetType::Cash;
    seed.assetTicker = toString(Currency::EUR);
    seed.quantity = 10'000.0;
    seed.pricePerUnit = 1.0;
    seed.fees = 0.0;
    seed.settlementCurrency = Currency::EUR;
    std::vector<Transaction> allTxns;
    allTxns.push_back(seed);
    allTxns.insert(allTxns.end(), txns.begin(), txns.end());
    bundle.service->importTransactions("pea", std::move(allTxns));

    const int64_t firstMs = txns.front().timestampsMs;
    const int64_t lastMs = txns.back().timestampsMs;

    TimeSeries series = bundle.service->valueSeries("pea", firstMs, lastMs, kWeekMs);
    assertValidValueSeries(series, "PEA");
    EXPECT_GT(series.size(), 4u) << "PEA: expected more than 4 weekly data points";

    // Exercise the GetPortfoliosByIds → load + totalValue path.
    // This calls assetService_->load(ticker) → YFinanceEquityProvider::fetch,
    // which is the path where currencyFromString can throw for unsupported currencies.
    const auto portfolio = bundle.service->load("pea");
    const double totalAtEnd = bundle.service->totalValue(portfolio, lastMs);
    EXPECT_GT(totalAtEnd, 0.0) << "PEA: totalValue at end of history should be positive";
}

// ============================================================
// NISA — US-listed ETF portfolio (USD-denominated, no FX)
// ============================================================

TEST(PortfolioTrackingLive, NISA_ValueSeriesFromCSV) {
    auto bundle = makeLiveBundle("nisa");

    auto txns = YahooFinanceImporter::parse(kResourcesDir / "portfolio NISA.csv",
                                            YahooFinanceImporter::Config{Currency::USD, nullptr});
    ASSERT_FALSE(txns.empty()) << "No transactions parsed from NISA CSV";

    // Use the full service layer — same path as RequestAddTransactionByCsv.
    bundle.service->createNew("nisa", "NISA", Currency::USD);
    bundle.service->importTransactions("nisa", txns);

    const int64_t firstMs = txns.front().timestampsMs;
    const int64_t lastMs = txns.back().timestampsMs;

    TimeSeries series = bundle.service->valueSeries("nisa", firstMs, lastMs, kWeekMs);
    assertValidValueSeries(series, "NISA");
    EXPECT_GT(series.size(), 4u) << "NISA: expected more than 4 weekly data points";

    // Exercise the GetPortfoliosByIds → load + totalValue path.
    const auto portfolio = bundle.service->load("nisa");
    const double totalAtEnd = bundle.service->totalValue(portfolio, lastMs);
    EXPECT_GT(totalAtEnd, 0.0) << "NISA: totalValue at end of history should be positive";
}
