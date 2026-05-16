// Copyright (c) 2026 JBBLET. All Rights Reserved.
//
// Integration tests that parse real Yahoo Finance CSV exports and call
// PortfolioService::valueSeries() with live yfinance price data.
// These tests require network access and are labelled "integration".
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

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
    std::unique_ptr<PortfolioService>       service;
    std::shared_ptr<CSVPortfolioRepository> portfolioRepo;
    std::filesystem::path                   repoDir;

    ~LiveBundle() { std::filesystem::remove_all(repoDir); }
};

LiveBundle makeLiveBundle(const std::string& testName) {
    auto repoDir = std::filesystem::temp_directory_path() / ("finapp_live_" + testName);
    std::filesystem::remove_all(repoDir);
    std::filesystem::create_directories(repoDir);

    // Time-series: live yfinance loader backed by an in-memory cache.
    auto innerRepo  = std::make_shared<InMemoryTimeSeriesRepository>();
    auto cachedRepo = std::make_shared<CachedTimeSeriesRepository>(innerRepo);
    auto tsLoader   = std::make_shared<YFinanceProvider>(
        "/home/jbblet/user/Documents/Projects/FinLib/.venv/bin/python",
        FINAPP_PYTHON_DIR "/YFinanceFetcher.py");
    auto tsService = std::make_shared<TimeSeriesService>(cachedRepo, tsLoader);

    // Asset: in-memory repo + live yfinance equity provider to resolve denomination.
    auto equityRepo     = std::make_shared<InMemoryAssetRepository>();
    auto equityProvider = std::make_shared<YFinanceEquityProvider>();
    std::unordered_map<AssetType, std::shared_ptr<IAssetRepository>> repos     = {{AssetType::Equity, equityRepo}};
    std::unordered_map<AssetType, std::shared_ptr<IAssetProvider>>   providers = {{AssetType::Equity, equityProvider}};
    auto assetService = std::make_shared<AssetService>(tsService, std::move(repos), std::move(providers));

    // FX: empty repo — both portfolios use a single base currency throughout.
    auto fxRepo    = std::make_shared<InMemoryFXRepository>();
    auto fxService = std::make_shared<FXService>(tsService, fxRepo);

    auto portfolioRepo = std::make_shared<CSVPortfolioRepository>(repoDir);
    auto service       = std::make_unique<PortfolioService>(portfolioRepo, assetService, fxService);

    return {std::move(service), std::move(portfolioRepo), repoDir};
}

void assertValidValueSeries(const TimeSeries& series, const std::string& label) {
    ASSERT_GT(series.size(), 0u) << label << ": series is empty";

    for (size_t i = 0; i < series.getValues().size(); ++i) {
        EXPECT_TRUE(std::isfinite(series.getValues()[i]))
            << label << ": non-finite value at index " << i;
        EXPECT_GE(series.getValues()[i], 0.0)
            << label << ": negative value at index " << i;
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

    // Seed 30 EUR to cover small interest/rounding credits not captured in the Yahoo export.
    bundle.portfolioRepo->saveSnapshot(
        PortfolioSnapshot{"PEA", Currency::EUR, 0, "pea", {}, {{Currency::EUR, 30.0}}});
    bundle.portfolioRepo->appendTransactions("pea", txns);

    const int64_t firstMs = txns.front().timestampsMs;
    const int64_t lastMs  = txns.back().timestampsMs;

    TimeSeries series = bundle.service->valueSeries("pea", firstMs, lastMs, kWeekMs);
    assertValidValueSeries(series, "PEA");
    EXPECT_GT(series.size(), 4u) << "PEA: expected more than 4 weekly data points";
}

// ============================================================
// NISA — US-listed ETF portfolio (USD-denominated, no FX)
// ============================================================

TEST(PortfolioTrackingLive, NISA_ValueSeriesFromCSV) {
    auto bundle = makeLiveBundle("nisa");

    auto txns = YahooFinanceImporter::parse(kResourcesDir / "portfolio NISA.csv",
                                            YahooFinanceImporter::Config{Currency::USD, nullptr});
    ASSERT_FALSE(txns.empty()) << "No transactions parsed from NISA CSV";

    bundle.portfolioRepo->saveSnapshot(PortfolioSnapshot{"NISA", Currency::USD, 0, "nisa", {}, {}});
    bundle.portfolioRepo->appendTransactions("nisa", txns);

    const int64_t firstMs = txns.front().timestampsMs;
    const int64_t lastMs  = txns.back().timestampsMs;

    TimeSeries series = bundle.service->valueSeries("nisa", firstMs, lastMs, kWeekMs);
    assertValidValueSeries(series, "NISA");
    EXPECT_GT(series.size(), 4u) << "NISA: expected more than 4 weekly data points";
}
