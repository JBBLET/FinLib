// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/asset/Equity.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finapp/finance/common/AssetId.hpp"
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

constexpr int64_t kDay = 86'400'000;

// STCK: 10 EUR/share for days 0-5, halves to 5 EUR/share for days 6-8.
// The step-down simulates a 2:1 split (quantity doubles, price halves) so
// portfolio value is preserved across the split tick.
TimeSeries makeSTCKPriceSeries() {
    std::vector<int64_t> ts;
    std::vector<double>  vs;
    for (int i = 0; i <= 5; ++i) { ts.push_back(i * kDay); vs.push_back(10.0); }
    for (int i = 6; i <= 8; ++i) { ts.push_back(i * kDay); vs.push_back(5.0); }
    return TimeSeries("STCK", std::move(ts), std::move(vs));
}

class PortfolioTrackingMathTest : public ::testing::Test {
 protected:
    std::shared_ptr<InMemoryTimeSeriesRepository> innerRepo;
    std::shared_ptr<CachedTimeSeriesRepository>   cachedRepo;
    std::shared_ptr<FakeTimeSeriesLoader>          tsLoader;
    std::shared_ptr<TimeSeriesService>             tsService;
    std::shared_ptr<InMemoryAssetRepository>       equityRepo;
    std::shared_ptr<FakeAssetProvider>             equityProvider;
    std::shared_ptr<AssetService>                  assetService;
    std::shared_ptr<InMemoryFXRepository>          fxRepo;
    std::shared_ptr<FXService>                     fxService;
    std::shared_ptr<InMemoryPortfolioRepository>   portfolioRepo;
    std::unique_ptr<PortfolioService>              service;

    void SetUp() override {
        innerRepo  = std::make_shared<InMemoryTimeSeriesRepository>();
        cachedRepo = std::make_shared<CachedTimeSeriesRepository>(innerRepo);
        tsLoader   = std::make_shared<FakeTimeSeriesLoader>();
        tsService  = std::make_shared<TimeSeriesService>(cachedRepo, tsLoader);

        equityRepo     = std::make_shared<InMemoryAssetRepository>();
        equityProvider = std::make_shared<FakeAssetProvider>();
        std::unordered_map<AssetType, std::shared_ptr<IAssetRepository>> repos     = {{AssetType::Equity, equityRepo}};
        std::unordered_map<AssetType, std::shared_ptr<IAssetProvider>>   providers = {{AssetType::Equity, equityProvider}};
        assetService = std::make_shared<AssetService>(tsService, std::move(repos), std::move(providers));

        fxRepo    = std::make_shared<InMemoryFXRepository>();
        fxService = std::make_shared<FXService>(tsService, fxRepo);

        portfolioRepo = std::make_shared<InMemoryPortfolioRepository>();
        service       = std::make_unique<PortfolioService>(portfolioRepo, assetService, fxService);

        equityRepo->save(std::make_shared<Equity>("STCK", "Stock Inc.", Currency::EUR));
        tsLoader->setSeries("STCK", makeSTCKPriceSeries());

        // Empty snapshot at t=0; all transactions live in [1*kDay, 6*kDay].
        portfolioRepo->saveSnapshot(PortfolioSnapshot{"math_pf", Currency::EUR, 0, "math_pf", {}, {}});

        // Transaction sequence covering all six types.
        //
        // Day 1  Deposit   10 000 EUR               → cash = 10 000
        // Day 2  Buy       100 STCK @ 10, fees 5    → cost  1 005, cash =  8 995,  100 STCK
        // Day 3  Dividend  0.1 EUR/share             → +10 cash,    cash =  9 005,  100 STCK
        // Day 4  Sell       50 STCK @ 10, fees 5    → rev    495,   cash =  9 500,   50 STCK
        // Day 5  Withdrawal 500 EUR                  →               cash =  9 000,   50 STCK
        // Day 6  Split 2:1  (price → 5 EUR)          →               cash =  9 000,  100 STCK
        portfolioRepo->appendTransactions("math_pf", {
            {1 * kDay, TransactionType::Deposit,    AssetType::Cash,   "EUR",  10000.0, 1.0,  0.0, Currency::EUR},
            {2 * kDay, TransactionType::Buy,        AssetType::Equity, "STCK",   100.0, 10.0, 5.0, Currency::EUR},
            {3 * kDay, TransactionType::Dividend,   AssetType::Equity, "STCK",     0.0, 0.1,  0.0, Currency::EUR},
            {4 * kDay, TransactionType::Sell,       AssetType::Equity, "STCK",    50.0, 10.0, 5.0, Currency::EUR},
            {5 * kDay, TransactionType::Withdrawal, AssetType::Cash,   "EUR",    500.0, 1.0,  0.0, Currency::EUR},
            {6 * kDay, TransactionType::Split,      AssetType::Equity, "STCK",     2.0, 0.0,  0.0, Currency::EUR},
        });
    }
};

}  // namespace

// ============================================================
// valueSeries — exact value at every daily tick
// ============================================================

// Expected portfolio value (EUR) at each daily tick 0..8:
//
//  i | transaction applied         | positions     | cash   | value
//  --+-----------------------------+---------------+--------+-------
//  0 | (none — empty portfolio)    | —             |     0  |      0
//  1 | Deposit 10 000              | —             | 10 000 | 10 000
//  2 | Buy 100 STCK (fees 5)       | 100 × 10      |  8 995 |  9 995
//  3 | Dividend 0.1/share          | 100 × 10      |  9 005 | 10 005
//  4 | Sell 50 STCK (fees 5)       |  50 × 10      |  9 500 | 10 000
//  5 | Withdrawal 500              |  50 × 10      |  9 000 |  9 500
//  6 | Split 2:1, price → 5       | 100 ×  5      |  9 000 |  9 500
//  7 | (none)                      | 100 ×  5      |  9 000 |  9 500
//  8 | (none)                      | 100 ×  5      |  9 000 |  9 500
TEST_F(PortfolioTrackingMathTest, ValueSeriesExactPerTick) {
    TimeSeries series = service->valueSeries("math_pf", 0, 8 * kDay, kDay);

    ASSERT_EQ(series.size(), 9u);
    const std::vector<double> expected = {
        0.0, 10000.0, 9995.0, 10005.0, 10000.0, 9500.0, 9500.0, 9500.0, 9500.0};
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_NEAR(series.getValues()[i], expected[i], 1e-6) << "value mismatch at tick " << i;
    }
}

// ============================================================
// weightSeries — spot-check at tick 4 and verify sum == 1
// ============================================================

// At tick 4 (after Sell 50, before Withdrawal):
//   STCK:    50 × 10 = 500   → w = 0.05
//   CASH:EUR:          9 500  → w = 0.95
TEST_F(PortfolioTrackingMathTest, WeightSeriesExactAtTick4) {
    auto weights = service->weightSeries("math_pf", 0, 8 * kDay, kDay);

    ASSERT_TRUE(weights.contains("STCK"));
    ASSERT_TRUE(weights.contains("CASH:EUR"));

    EXPECT_NEAR(weights.at("STCK").getValues()[4],     0.05, 1e-9);
    EXPECT_NEAR(weights.at("CASH:EUR").getValues()[4], 0.95, 1e-9);

    // Weights must sum to 1 at every tick that has nonzero portfolio value.
    for (size_t i = 1; i < 9; ++i) {
        double sum = 0.0;
        for (const auto& [_, ts] : weights) sum += ts.getValues()[i];
        EXPECT_NEAR(sum, 1.0, 1e-9) << "weights don't sum to 1 at tick " << i;
    }
}

// ============================================================
// Split preserves portfolio value
// ============================================================

// tick 5 (pre-split): 50 STCK × 10 + 9 000 = 9 500
// tick 6 (post-split): 100 STCK × 5 + 9 000 = 9 500
TEST_F(PortfolioTrackingMathTest, SplitPreservesPortfolioValue) {
    TimeSeries series = service->valueSeries("math_pf", 0, 8 * kDay, kDay);
    EXPECT_NEAR(series.getValues()[5], series.getValues()[6], 1e-9);
}

// ============================================================
// Fees reduce portfolio value
// ============================================================

// At constant price, a frictionless Buy + frictionless Sell is value-neutral.
// With fees, the two rounds of fees (5 + 5 = 10 EUR) permanently reduce the portfolio.
// After the Sell (tick 4) the portfolio is 10 000 – 10 = 9 990 below the Deposit level.
// Then a Withdrawal of 500 EUR brings it to 9 500, which is 500 below the net-of-fees value.
TEST_F(PortfolioTrackingMathTest, FeesReducePortfolioValue) {
    TimeSeries series = service->valueSeries("math_pf", 0, 8 * kDay, kDay);

    // After Buy (tick 2): 9 995 = deposit 10 000 - fee 5
    EXPECT_NEAR(series.getValues()[2], 9995.0, 1e-6);

    // After Sell (tick 4): 10 000 = 9 995 + dividend 10 - sell-fee 5
    EXPECT_NEAR(series.getValues()[4], 10000.0, 1e-6);
}
