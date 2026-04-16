// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/asset/Equity.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finapp/finance/common/AssetId.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finapp/finance/portfolio/PortfolioSnapshot.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"
#include "finapp/service/AssetService.hpp"
#include "finapp/service/FXService.hpp"
#include "finapp/service/PortfolioService.hpp"
#include "finlib/common/utils/TimeSeriesUtils.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/implementation/CachedTimeSeriesRepository.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"
#include "support/service_test_fakes.hpp"

using namespace finance;
using namespace finance::test;

namespace {

constexpr int64_t kDay = 86'400'000;

class PortfolioServiceTest : public ::testing::Test {
 protected:
    std::shared_ptr<InMemoryTimeSeriesRepository> innerRepo;
    std::shared_ptr<CachedTimeSeriesRepository> cachedRepo;
    std::shared_ptr<FakeTimeSeriesLoader> tsProvider;
    std::shared_ptr<TimeSeriesService> tsService;

    std::shared_ptr<InMemoryAssetRepository> equityRepo;
    std::shared_ptr<FakeAssetProvider> equityProvider;
    std::shared_ptr<AssetService> assetService;

    std::shared_ptr<InMemoryFXRepository> fxRepo;
    std::shared_ptr<FXService> fxService;

    std::shared_ptr<InMemoryPortfolioRepository> portfolioRepo;
    std::unique_ptr<PortfolioService> service;

    void SetUp() override {
        innerRepo = std::make_shared<InMemoryTimeSeriesRepository>();
        cachedRepo = std::make_shared<CachedTimeSeriesRepository>(innerRepo);
        tsProvider = std::make_shared<FakeTimeSeriesLoader>();
        tsService = std::make_shared<TimeSeriesService>(cachedRepo, tsProvider);

        equityRepo = std::make_shared<InMemoryAssetRepository>();
        equityProvider = std::make_shared<FakeAssetProvider>();
        std::unordered_map<AssetType, std::shared_ptr<IAssetRepository>> repos = {{AssetType::Equity, equityRepo}};
        std::unordered_map<AssetType, std::shared_ptr<IAssetProvider>> providers = {
            {AssetType::Equity, equityProvider}};
        assetService = std::make_shared<AssetService>(tsService, std::move(repos), std::move(providers));

        fxRepo = std::make_shared<InMemoryFXRepository>();
        fxService = std::make_shared<FXService>(tsService, fxRepo);

        portfolioRepo = std::make_shared<InMemoryPortfolioRepository>();
        service = std::make_unique<PortfolioService>(portfolioRepo, assetService, fxService);
    }

    // Helper — registers an equity with the asset service and seeds a constant price series
    // covering the entire window used by valueSeries / weightSeries tests.
    void registerEquity(const std::string& ticker, Currency denom, double price) {
        auto asset = std::make_shared<Equity>(ticker, ticker, denom);
        equityRepo->save(asset);
        tsProvider->setSeries(ticker, makeFlatSeries(ticker, 0, 30 * kDay, kDay, price));
    }
};

}  // namespace

// ============================================================
// load / save
// ============================================================

TEST_F(PortfolioServiceTest, LoadReconstructsFromSnapshot) {
    PortfolioSnapshot snap{"My Portfolio",
                            Currency::USD,
                            5 * kDay,
                            "pf1",
                            {SnapshotPosition{AssetId{AssetType::Equity, "AAPL"}, 10.0}},
                            {{Currency::USD, 1000.0}}};
    portfolioRepo->saveSnapshot(snap);

    Portfolio p = service->load("pf1");
    EXPECT_EQ(p.id(), "pf1");
    EXPECT_EQ(p.name(), "My Portfolio");
    EXPECT_EQ(p.baseCurrency(), Currency::USD);
    ASSERT_EQ(p.positions().size(), 1);
    EXPECT_EQ(p.positions()[0].assetId.ticker, "AAPL");
    EXPECT_DOUBLE_EQ(p.positions()[0].quantity, 10.0);
    EXPECT_DOUBLE_EQ(p.cashBalance(Currency::USD), 1000.0);
}

TEST_F(PortfolioServiceTest, LoadAppliesPostSnapshotTransactions) {
    PortfolioSnapshot snap{"pf", Currency::USD, 0, "pf1", {}, {{Currency::USD, 10000.0}}};
    portfolioRepo->saveSnapshot(snap);
    portfolioRepo->appendTransactions(
        "pf1", {Transaction{1 * kDay, TransactionType::Buy, AssetType::Equity, "AAPL", 10.0, 150.0, 0.0, Currency::USD}});

    Portfolio p = service->load("pf1");
    ASSERT_EQ(p.positions().size(), 1);
    EXPECT_DOUBLE_EQ(p.positions()[0].quantity, 10.0);
    EXPECT_DOUBLE_EQ(p.cashBalance(Currency::USD), 10000.0 - 10.0 * 150.0);
}

TEST_F(PortfolioServiceTest, LoadThrowsWhenSnapshotMissing) {
    EXPECT_THROW(service->load("nope"), std::runtime_error);
}

TEST_F(PortfolioServiceTest, SavePersistsSnapshot) {
    PortfolioSnapshot snap{"pf", Currency::USD, 0, "pf1", {}, {{Currency::USD, 1000.0}}};
    portfolioRepo->saveSnapshot(snap);
    Portfolio p = service->load("pf1");

    service->save(p, 5 * kDay);
    auto reloaded = portfolioRepo->loadLatestSnapshot("pf1");
    ASSERT_TRUE(reloaded.has_value());
    EXPECT_EQ(reloaded->timestampMs, 5 * kDay);
    EXPECT_EQ(reloaded->name, "pf");
    EXPECT_EQ(reloaded->baseCurrency, Currency::USD);
}

// ============================================================
// totalValue
// ============================================================

TEST_F(PortfolioServiceTest, TotalValueSumsPositionsAndCash) {
    registerEquity("AAPL", Currency::USD, 200.0);
    PortfolioSnapshot snap{"pf",
                            Currency::USD,
                            0,
                            "pf1",
                            {SnapshotPosition{AssetId{AssetType::Equity, "AAPL"}, 10.0}},
                            {{Currency::USD, 500.0}}};
    portfolioRepo->saveSnapshot(snap);
    Portfolio p = service->load("pf1");

    double total = service->totalValue(p, 2 * kDay);
    EXPECT_DOUBLE_EQ(total, 10.0 * 200.0 + 500.0);
}

TEST_F(PortfolioServiceTest, TotalValueAppliesFXForNonBaseCash) {
    registerEquity("AAPL", Currency::USD, 100.0);
    fxRepo->save(FXInfos{Currency::EUR, Currency::USD, "EURUSD"});
    tsProvider->setSeries("EURUSD", makeFlatSeries("EURUSD", 0, 30 * kDay, kDay, 1.10));

    PortfolioSnapshot snap{"pf",
                            Currency::USD,
                            0,
                            "pf1",
                            {SnapshotPosition{AssetId{AssetType::Equity, "AAPL"}, 1.0}},
                            {{Currency::USD, 100.0}, {Currency::EUR, 100.0}}};
    portfolioRepo->saveSnapshot(snap);
    Portfolio p = service->load("pf1");

    double total = service->totalValue(p, 2 * kDay);
    // 1 * 100 + 100 USD + 100 EUR * 1.10 = 310
    EXPECT_DOUBLE_EQ(total, 310.0);
}

TEST_F(PortfolioServiceTest, TotalValueAppliesFXForNonBasePosition) {
    registerEquity("VOD", Currency::GBP, 50.0);
    fxRepo->save(FXInfos{Currency::GBP, Currency::USD, "GBPUSD"});
    tsProvider->setSeries("GBPUSD", makeFlatSeries("GBPUSD", 0, 30 * kDay, kDay, 1.25));

    PortfolioSnapshot snap{"pf",
                            Currency::USD,
                            0,
                            "pf1",
                            {SnapshotPosition{AssetId{AssetType::Equity, "VOD"}, 4.0}},
                            {{Currency::USD, 0.0}}};
    portfolioRepo->saveSnapshot(snap);
    Portfolio p = service->load("pf1");

    double total = service->totalValue(p, 2 * kDay);
    // 4 * 50 GBP * 1.25 = 250 USD
    EXPECT_DOUBLE_EQ(total, 250.0);
}

// ============================================================
// weights
// ============================================================

TEST_F(PortfolioServiceTest, WeightsSumToOne) {
    registerEquity("AAPL", Currency::USD, 100.0);
    registerEquity("MSFT", Currency::USD, 200.0);
    PortfolioSnapshot snap{"pf",
                            Currency::USD,
                            0,
                            "pf1",
                            {SnapshotPosition{AssetId{AssetType::Equity, "AAPL"}, 5.0},
                             SnapshotPosition{AssetId{AssetType::Equity, "MSFT"}, 5.0}},
                            {{Currency::USD, 500.0}}};
    portfolioRepo->saveSnapshot(snap);
    Portfolio p = service->load("pf1");

    auto weights = service->weights(p, 2 * kDay);
    double sum = 0.0;
    for (const auto& [_, w] : weights) sum += w;
    EXPECT_NEAR(sum, 1.0, 1e-9);
    // 5*100=500, 5*200=1000, cash=500 → total 2000
    EXPECT_NEAR(weights.at("AAPL"), 0.25, 1e-9);
    EXPECT_NEAR(weights.at("MSFT"), 0.5, 1e-9);
    EXPECT_NEAR(weights.at("CASH:USD"), 0.25, 1e-9);
}

TEST_F(PortfolioServiceTest, WeightsHandlesEmptyPortfolio) {
    PortfolioSnapshot snap{"pf", Currency::USD, 0, "pf1", {}, {}};
    portfolioRepo->saveSnapshot(snap);
    Portfolio p = service->load("pf1");

    auto weights = service->weights(p, 2 * kDay);
    EXPECT_TRUE(weights.empty());
}

// ============================================================
// rebalance
// ============================================================

TEST_F(PortfolioServiceTest, RebalanceEmptyTargetsReturnsEmpty) {
    registerEquity("AAPL", Currency::USD, 100.0);
    PortfolioSnapshot snap{"pf",
                            Currency::USD,
                            0,
                            "pf1",
                            {SnapshotPosition{AssetId{AssetType::Equity, "AAPL"}, 5.0}},
                            {{Currency::USD, 500.0}}};
    portfolioRepo->saveSnapshot(snap);
    Portfolio p = service->load("pf1");

    auto txs = service->rebalance(p, 2 * kDay);
    EXPECT_TRUE(txs.empty());
}

TEST_F(PortfolioServiceTest, RebalanceProducesBuyAndSellDeltas) {
    registerEquity("AAPL", Currency::USD, 100.0);
    registerEquity("MSFT", Currency::USD, 100.0);

    // Total 1000. Currently 100% AAPL. Target 50/50 → sell 5 AAPL, buy 5 MSFT.
    PortfolioSnapshot snap{"pf",
                            Currency::USD,
                            0,
                            "pf1",
                            {SnapshotPosition{AssetId{AssetType::Equity, "AAPL"}, 10.0}},
                            {{Currency::USD, 0.0}}};
    portfolioRepo->saveSnapshot(snap);
    Portfolio p = service->load("pf1");
    p.setTargetAllocations({{AssetId{AssetType::Equity, "AAPL"}, 0.5}, {AssetId{AssetType::Equity, "MSFT"}, 0.5}});

    auto txs = service->rebalance(p, 2 * kDay);
    ASSERT_EQ(txs.size(), 2);

    bool sawSell = false, sawBuy = false;
    for (const auto& tx : txs) {
        if (tx.assetTicker == "AAPL") {
            EXPECT_EQ(tx.type, TransactionType::Sell);
            EXPECT_DOUBLE_EQ(tx.quantity, 5.0);
            sawSell = true;
        }
        if (tx.assetTicker == "MSFT") {
            EXPECT_EQ(tx.type, TransactionType::Buy);
            EXPECT_DOUBLE_EQ(tx.quantity, 5.0);
            sawBuy = true;
        }
    }
    EXPECT_TRUE(sawSell);
    EXPECT_TRUE(sawBuy);
}

// ============================================================
// valueSeries
// ============================================================

TEST_F(PortfolioServiceTest, ValueSeriesRangeOverloadConstantPrices) {
    registerEquity("AAPL", Currency::USD, 100.0);
    PortfolioSnapshot snap{"pf",
                            Currency::USD,
                            0,
                            "pf1",
                            {SnapshotPosition{AssetId{AssetType::Equity, "AAPL"}, 2.0}},
                            {{Currency::USD, 50.0}}};
    portfolioRepo->saveSnapshot(snap);

    TimeSeries series = service->valueSeries("pf1", 0, 3 * kDay, kDay);
    ASSERT_EQ(series.size(), 4);
    for (double v : series.getValues()) {
        EXPECT_DOUBLE_EQ(v, 2.0 * 100.0 + 50.0);
    }
}

TEST_F(PortfolioServiceTest, ValueSeriesAppliesTransactionsAtTick) {
    registerEquity("AAPL", Currency::USD, 100.0);
    PortfolioSnapshot snap{"pf", Currency::USD, 0, "pf1", {}, {{Currency::USD, 1000.0}}};
    portfolioRepo->saveSnapshot(snap);
    portfolioRepo->appendTransactions(
        "pf1", {Transaction{2 * kDay, TransactionType::Buy, AssetType::Equity, "AAPL", 5.0, 100.0, 0.0, Currency::USD}});

    TimeSeries series = service->valueSeries("pf1", 0, 3 * kDay, kDay);
    ASSERT_EQ(series.size(), 4);
    // Total value before and after the trade is unchanged at constant prices: 1000.
    for (double v : series.getValues()) {
        EXPECT_DOUBLE_EQ(v, 1000.0);
    }
}

TEST_F(PortfolioServiceTest, ValueSeriesSharedTimestampsKeepsPointerAlignment) {
    registerEquity("AAPL", Currency::USD, 100.0);
    PortfolioSnapshot snap{"pf",
                            Currency::USD,
                            0,
                            "pf1",
                            {SnapshotPosition{AssetId{AssetType::Equity, "AAPL"}, 2.0}},
                            {{Currency::USD, 50.0}}};
    portfolioRepo->saveSnapshot(snap);

    auto timestamps = common::utils::timeSeries::makeRegularTimestamps(0, 3 * kDay, kDay);
    TimeSeries series = service->valueSeries("pf1", timestamps);
    ASSERT_EQ(series.size(), 4);
    EXPECT_EQ(series.getSharedTimestamps().get(), timestamps.get());
}

// ============================================================
// weightSeries
// ============================================================

TEST_F(PortfolioServiceTest, WeightSeriesNormalizesToOne) {
    registerEquity("AAPL", Currency::USD, 100.0);
    registerEquity("MSFT", Currency::USD, 100.0);
    PortfolioSnapshot snap{"pf",
                            Currency::USD,
                            0,
                            "pf1",
                            {SnapshotPosition{AssetId{AssetType::Equity, "AAPL"}, 5.0},
                             SnapshotPosition{AssetId{AssetType::Equity, "MSFT"}, 5.0}},
                            {{Currency::USD, 0.0}}};
    portfolioRepo->saveSnapshot(snap);

    auto weights = service->weightSeries("pf1", 0, 3 * kDay, kDay);
    ASSERT_EQ(weights.size(), 3);  // AAPL + MSFT + CASH:USD (zero-valued)
    ASSERT_TRUE(weights.contains("AAPL"));
    ASSERT_TRUE(weights.contains("MSFT"));

    for (size_t i = 0; i < weights.at("AAPL").size(); ++i) {
        double sum = weights.at("AAPL").getValues()[i] + weights.at("MSFT").getValues()[i] +
                     weights.at("CASH:USD").getValues()[i];
        EXPECT_NEAR(sum, 1.0, 1e-9);
        EXPECT_NEAR(weights.at("AAPL").getValues()[i], 0.5, 1e-9);
        EXPECT_NEAR(weights.at("MSFT").getValues()[i], 0.5, 1e-9);
    }
}

TEST_F(PortfolioServiceTest, WeightSeriesSharedTimestampsKeepsPointerAlignment) {
    registerEquity("AAPL", Currency::USD, 100.0);
    PortfolioSnapshot snap{"pf",
                            Currency::USD,
                            0,
                            "pf1",
                            {SnapshotPosition{AssetId{AssetType::Equity, "AAPL"}, 2.0}},
                            {{Currency::USD, 50.0}}};
    portfolioRepo->saveSnapshot(snap);

    auto timestamps = common::utils::timeSeries::makeRegularTimestamps(0, 3 * kDay, kDay);
    auto weights = service->weightSeries("pf1", timestamps);
    ASSERT_FALSE(weights.empty());
    for (const auto& [_, ts] : weights) {
        EXPECT_EQ(ts.getSharedTimestamps().get(), timestamps.get());
    }
}

TEST_F(PortfolioServiceTest, ValueSeriesEmptyTimestampsThrows) {
    PortfolioSnapshot snap{"pf", Currency::USD, 0, "pf1", {}, {{Currency::USD, 1.0}}};
    portfolioRepo->saveSnapshot(snap);
    auto empty = std::make_shared<const std::vector<int64_t>>();
    EXPECT_THROW(service->valueSeries("pf1", empty), std::invalid_argument);
}
