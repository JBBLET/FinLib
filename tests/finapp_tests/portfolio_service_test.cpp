// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finapp/data/repository/implementation/InMemoryRepository/InMemoryAssetRepository.hpp"
#include "finapp/data/repository/implementation/InMemoryRepository/InMemoryFXRepository.hpp"
#include "finapp/data/repository/implementation/InMemoryRepository/InMemoryPortfolioRepository.hpp"
#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/asset/Equity.hpp"
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
#include "finlib/data/implementation/InMemoryTimeSeriesRepository.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"
#include "support/service_test_fakes.hpp"

using ts::CachedTimeSeriesRepository;
using ts::InMemoryTimeSeriesRepository;
using ts::TimeSeries;
using ts::TimeSeriesService;

namespace {

constexpr int64_t kDay = 86'400'000;

class PortfolioServiceTest : public ::testing::Test {
 protected:
    std::shared_ptr<InMemoryTimeSeriesRepository> innerRepo;
    std::shared_ptr<CachedTimeSeriesRepository> cachedRepo;
    std::shared_ptr<finapp::test::FakeTimeSeriesLoader> tsProvider;
    std::shared_ptr<TimeSeriesService> tsService;

    std::shared_ptr<finapp::InMemoryAssetRepository> equityRepo;
    std::shared_ptr<finapp::test::FakeAssetProvider> equityProvider;
    std::shared_ptr<finapp::AssetService> assetService;

    std::shared_ptr<finapp::InMemoryFXRepository> fxRepo;
    std::shared_ptr<finapp::FXService> fxService;

    std::shared_ptr<finapp::InMemoryPortfolioRepository> portfolioRepo;
    std::unique_ptr<finapp::PortfolioService> service;

    void SetUp() override {
        innerRepo = std::make_shared<InMemoryTimeSeriesRepository>();
        cachedRepo = std::make_shared<CachedTimeSeriesRepository>(innerRepo);
        tsProvider = std::make_shared<finapp::test::FakeTimeSeriesLoader>();
        tsService = std::make_shared<TimeSeriesService>(cachedRepo, tsProvider);

        equityRepo = std::make_shared<finapp::InMemoryAssetRepository>();
        equityProvider = std::make_shared<finapp::test::FakeAssetProvider>();
        std::unordered_map<finance::AssetType, std::shared_ptr<finapp::IAssetRepository>> repos = {
            {finance::AssetType::Equity, equityRepo}};
        std::unordered_map<finance::AssetType, std::shared_ptr<finapp::IAssetProvider>> providers = {
            {finance::AssetType::Equity, equityProvider}};
        assetService = std::make_shared<finapp::AssetService>(tsService, std::move(repos), std::move(providers));

        fxRepo = std::make_shared<finapp::InMemoryFXRepository>();
        fxService = std::make_shared<finapp::FXService>(tsService, fxRepo);

        portfolioRepo = std::make_shared<finapp::InMemoryPortfolioRepository>();
        service = std::make_unique<finapp::PortfolioService>(portfolioRepo, assetService, fxService);
    }

    // Helper — registers an equity with the asset service and seeds a constant price series
    // covering the entire window used by valueSeries / weightSeries tests.
    void registerEquity(const std::string& ticker, finance::Currency denom, double price) {
        auto asset = std::make_shared<finance::Equity>(ticker, ticker, denom);
        equityRepo->save(asset);
        tsProvider->setSeries(ticker, finapp::test::makeFlatSeries(ticker, 0, 30 * kDay, kDay, price));
    }
};

}  // namespace

// ============================================================
// load / save
// ============================================================

TEST_F(PortfolioServiceTest, LoadReconstructsFromSnapshot) {
    finance::PortfolioSnapshot snap{
        "My Portfolio",
        finance::Currency::USD,
        5 * kDay,
        "pf1",
        {finance::SnapshotPosition{finance::AssetId{finance::AssetType::Equity, "AAPL"}, 10.0}},
        {{finance::Currency::USD, 1000.0}}};
    portfolioRepo->saveSnapshot(snap);

    finance::Portfolio p = service->load("pf1");
    EXPECT_EQ(p.id(), "pf1");
    EXPECT_EQ(p.name(), "My Portfolio");
    EXPECT_EQ(p.baseCurrency(), finance::Currency::USD);
    ASSERT_EQ(p.positions().size(), 1);
    EXPECT_EQ(p.positions()[0].assetId.ticker, "AAPL");
    EXPECT_DOUBLE_EQ(p.positions()[0].quantity, 10.0);
    EXPECT_DOUBLE_EQ(p.cashBalance(finance::Currency::USD), 1000.0);
}

TEST_F(PortfolioServiceTest, LoadAppliesPostSnapshotTransactions) {
    finance::PortfolioSnapshot snap{"pf", finance::Currency::USD, 0, "pf1", {}, {{finance::Currency::USD, 10000.0}}};
    portfolioRepo->saveSnapshot(snap);
    portfolioRepo->appendTransactions("pf1",
                                      {finance::Transaction{"tx-load-1",
                                                            1 * kDay,
                                                            finance::TransactionType::Buy,
                                                            finance::AssetType::Equity,
                                                            "AAPL",
                                                            10.0,
                                                            150.0,
                                                            0.0,
                                                            finance::Currency::USD}});

    finance::Portfolio p = service->load("pf1");
    ASSERT_EQ(p.positions().size(), 1);
    EXPECT_DOUBLE_EQ(p.positions()[0].quantity, 10.0);
    EXPECT_DOUBLE_EQ(p.cashBalance(finance::Currency::USD), 10000.0 - 10.0 * 150.0);
}

TEST_F(PortfolioServiceTest, LoadThrowsWhenSnapshotMissing) { EXPECT_THROW(service->load("nope"), std::runtime_error); }

TEST_F(PortfolioServiceTest, SavePersistsSnapshot) {
    finance::PortfolioSnapshot snap{"pf", finance::Currency::USD, 0, "pf1", {}, {{finance::Currency::USD, 1000.0}}};
    portfolioRepo->saveSnapshot(snap);
    finance::Portfolio p = service->load("pf1");

    service->save(p, 5 * kDay);
    auto reloaded = portfolioRepo->loadLatestSnapshot("pf1");
    ASSERT_TRUE(reloaded.has_value());
    EXPECT_EQ(reloaded->timestampMs, 5 * kDay);
    EXPECT_EQ(reloaded->name, "pf");
    EXPECT_EQ(reloaded->baseCurrency, finance::Currency::USD);
}

// ============================================================
// computeOverviewAtTs — totalValue
// ============================================================

TEST_F(PortfolioServiceTest, TotalValueSumsPositionsAndCash) {
    registerEquity("AAPL", finance::Currency::USD, 200.0);
    finance::PortfolioSnapshot snap{
        "pf",
        finance::Currency::USD,
        0,
        "pf1",
        {finance::SnapshotPosition{finance::AssetId{finance::AssetType::Equity, "AAPL"}, 10.0}},
        {{finance::Currency::USD, 500.0}}};
    portfolioRepo->saveSnapshot(snap);

    double total = service->computeOverviewAtTs("pf1", 2 * kDay).totalValue;
    EXPECT_DOUBLE_EQ(total, 10.0 * 200.0 + 500.0);
}

TEST_F(PortfolioServiceTest, TotalValueAppliesFXForNonBaseCash) {
    registerEquity("AAPL", finance::Currency::USD, 100.0);
    fxRepo->save(finapp::FXInfos{finance::Currency::EUR, finance::Currency::USD, "EURUSD"});
    tsProvider->setSeries("EURUSD", finapp::test::makeFlatSeries("EURUSD", 0, 30 * kDay, kDay, 1.10));

    finance::PortfolioSnapshot snap{
        "pf",
        finance::Currency::USD,
        0,
        "pf1",
        {finance::SnapshotPosition{finance::AssetId{finance::AssetType::Equity, "AAPL"}, 1.0}},
        {{finance::Currency::USD, 100.0}, {finance::Currency::EUR, 100.0}}};
    portfolioRepo->saveSnapshot(snap);

    double total = service->computeOverviewAtTs("pf1", 2 * kDay).totalValue;
    // 1 * 100 + 100 USD + 100 EUR * 1.10 = 310
    EXPECT_DOUBLE_EQ(total, 310.0);
}

TEST_F(PortfolioServiceTest, TotalValueAppliesFXForNonBasePosition) {
    registerEquity("VOD", finance::Currency::GBP, 50.0);
    fxRepo->save(finapp::FXInfos{finance::Currency::GBP, finance::Currency::USD, "GBPUSD"});
    tsProvider->setSeries("GBPUSD", finapp::test::makeFlatSeries("GBPUSD", 0, 30 * kDay, kDay, 1.25));

    finance::PortfolioSnapshot snap{
        "pf",
        finance::Currency::USD,
        0,
        "pf1",
        {finance::SnapshotPosition{finance::AssetId{finance::AssetType::Equity, "VOD"}, 4.0}},
        {{finance::Currency::USD, 0.0}}};
    portfolioRepo->saveSnapshot(snap);

    double total = service->computeOverviewAtTs("pf1", 2 * kDay).totalValue;
    // 4 * 50 GBP * 1.25 = 250 USD
    EXPECT_DOUBLE_EQ(total, 250.0);
}

// ============================================================
// computeOverviewAtTs — weights
// ============================================================

TEST_F(PortfolioServiceTest, WeightsSumToOne) {
    registerEquity("AAPL", finance::Currency::USD, 100.0);
    registerEquity("MSFT", finance::Currency::USD, 200.0);
    finance::PortfolioSnapshot snap{
        "pf",
        finance::Currency::USD,
        0,
        "pf1",
        {finance::SnapshotPosition{finance::AssetId{finance::AssetType::Equity, "AAPL"}, 5.0},
         finance::SnapshotPosition{finance::AssetId{finance::AssetType::Equity, "MSFT"}, 5.0}},
        {{finance::Currency::USD, 500.0}}};
    portfolioRepo->saveSnapshot(snap);

    auto weights = service->computeOverviewAtTs("pf1", 2 * kDay).weights;
    double sum = 0.0;
    for (const auto& [_, w] : weights) sum += w;
    EXPECT_NEAR(sum, 1.0, 1e-9);
    // 5*100=500, 5*200=1000, cash=500 → total 2000
    EXPECT_NEAR(weights.at("AAPL"), 0.25, 1e-9);
    EXPECT_NEAR(weights.at("MSFT"), 0.5, 1e-9);
    EXPECT_NEAR(weights.at("CASH:USD"), 0.25, 1e-9);
}

TEST_F(PortfolioServiceTest, WeightsHandlesEmptyPortfolio) {
    finance::PortfolioSnapshot snap{"pf", finance::Currency::USD, 0, "pf1", {}, {}};
    portfolioRepo->saveSnapshot(snap);

    auto weights = service->computeOverviewAtTs("pf1", 2 * kDay).weights;
    EXPECT_TRUE(weights.empty());
}

// ============================================================
// valueSeries
// ============================================================

TEST_F(PortfolioServiceTest, ValueSeriesRangeOverloadConstantPrices) {
    registerEquity("AAPL", finance::Currency::USD, 100.0);
    finance::PortfolioSnapshot snap{
        "pf",
        finance::Currency::USD,
        0,
        "pf1",
        {finance::SnapshotPosition{finance::AssetId{finance::AssetType::Equity, "AAPL"}, 2.0}},
        {{finance::Currency::USD, 50.0}}};
    portfolioRepo->saveSnapshot(snap);

    TimeSeries series = service->valueSeries("pf1", 0, 3 * kDay, kDay);
    ASSERT_EQ(series.size(), 4);
    for (double v : series.getValues()) {
        EXPECT_DOUBLE_EQ(v, 2.0 * 100.0 + 50.0);
    }
}

TEST_F(PortfolioServiceTest, ValueSeriesAppliesTransactionsAtTick) {
    registerEquity("AAPL", finance::Currency::USD, 100.0);
    finance::PortfolioSnapshot snap{"pf", finance::Currency::USD, 0, "pf1", {}, {{finance::Currency::USD, 1000.0}}};
    portfolioRepo->saveSnapshot(snap);
    portfolioRepo->appendTransactions("pf1",
                                      {finance::Transaction{"tx-series-1",
                                                            2 * kDay,
                                                            finance::TransactionType::Buy,
                                                            finance::AssetType::Equity,
                                                            "AAPL",
                                                            5.0,
                                                            100.0,
                                                            0.0,
                                                            finance::Currency::USD}});

    TimeSeries series = service->valueSeries("pf1", 0, 3 * kDay, kDay);
    ASSERT_EQ(series.size(), 4);
    // Total value before and after the trade is unchanged at constant prices: 1000.
    for (double v : series.getValues()) {
        EXPECT_DOUBLE_EQ(v, 1000.0);
    }
}

TEST_F(PortfolioServiceTest, ValueSeriesSharedTimestampsKeepsPointerAlignment) {
    registerEquity("AAPL", finance::Currency::USD, 100.0);
    finance::PortfolioSnapshot snap{
        "pf",
        finance::Currency::USD,
        0,
        "pf1",
        {finance::SnapshotPosition{finance::AssetId{finance::AssetType::Equity, "AAPL"}, 2.0}},
        {{finance::Currency::USD, 50.0}}};
    portfolioRepo->saveSnapshot(snap);

    auto timestamps = ts::common::utils::timeSeries::makeRegularTimestamps(0, 3 * kDay, kDay);
    TimeSeries series = service->valueSeries("pf1", timestamps);
    ASSERT_EQ(series.size(), 4);
    EXPECT_EQ(series.getSharedTimestamps().get(), timestamps.get());
}

TEST_F(PortfolioServiceTest, ValueSeriesEmptyTimestampsThrows) {
    finance::PortfolioSnapshot snap{"pf", finance::Currency::USD, 0, "pf1", {}, {{finance::Currency::USD, 1.0}}};
    portfolioRepo->saveSnapshot(snap);
    auto empty = std::make_shared<const std::vector<int64_t>>();
    EXPECT_THROW(service->valueSeries("pf1", empty), std::invalid_argument);
}

// ============================================================
// listPortfolioIds / listTransactions
// ============================================================

TEST_F(PortfolioServiceTest, ListPortfolioIdsReturnsAll) {
    portfolioRepo->saveSnapshot({"pf-a", finance::Currency::USD, 0, "pf-a", {}, {}});
    portfolioRepo->saveSnapshot({"pf-b", finance::Currency::USD, 0, "pf-b", {}, {}});

    auto ids = service->listPortfolioIds();
    ASSERT_EQ(ids.size(), 2);
    std::sort(ids.begin(), ids.end());
    EXPECT_EQ(ids[0], "pf-a");
    EXPECT_EQ(ids[1], "pf-b");
}

TEST_F(PortfolioServiceTest, ListTransactionsDelegatesToRepository) {
    portfolioRepo->saveSnapshot({"pf", finance::Currency::USD, 0, "pf1", {}, {{finance::Currency::USD, 10000.0}}});
    portfolioRepo->appendTransactions("pf1",
                                      {
                                          finance::Transaction{"lt-1",
                                                               1 * kDay,
                                                               finance::TransactionType::Deposit,
                                                               finance::AssetType::Cash,
                                                               "USD",
                                                               1000.0,
                                                               1.0,
                                                               0.0,
                                                               finance::Currency::USD},
                                          finance::Transaction{"lt-2",
                                                               2 * kDay,
                                                               finance::TransactionType::Deposit,
                                                               finance::AssetType::Cash,
                                                               "USD",
                                                               2000.0,
                                                               1.0,
                                                               0.0,
                                                               finance::Currency::USD},
                                      });

    auto all = service->listTransactions("pf1", 0);
    ASSERT_EQ(all.size(), 2);

    auto filtered = service->listTransactions("pf1", 2 * kDay);
    ASSERT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered[0].id, "lt-2");
}

// ============================================================
// addTransaction
// ============================================================

TEST_F(PortfolioServiceTest, AddTransactionReturnsNonEmptyId) {
    portfolioRepo->saveSnapshot({"pf", finance::Currency::USD, 0, "pf1", {}, {{finance::Currency::USD, 10000.0}}});

    finance::Transaction tx{"",
                            kDay,
                            finance::TransactionType::Deposit,
                            finance::AssetType::Cash,
                            "USD",
                            5000.0,
                            1.0,
                            0.0,
                            finance::Currency::USD};
    std::string id = service->addTransaction("pf1", tx);

    EXPECT_FALSE(id.empty());
}

TEST_F(PortfolioServiceTest, AddTransactionPersistsWithGeneratedId) {
    portfolioRepo->saveSnapshot({"pf", finance::Currency::USD, 0, "pf1", {}, {{finance::Currency::USD, 10000.0}}});

    finance::Transaction tx{"",
                            kDay,
                            finance::TransactionType::Deposit,
                            finance::AssetType::Cash,
                            "USD",
                            5000.0,
                            1.0,
                            0.0,
                            finance::Currency::USD};
    std::string id = service->addTransaction("pf1", tx);

    auto txns = service->listTransactions("pf1", 0);
    ASSERT_EQ(txns.size(), 1);
    EXPECT_EQ(txns[0].id, id);
    EXPECT_NEAR(txns[0].quantity, 5000.0, 1e-9);
}

TEST_F(PortfolioServiceTest, AddTransactionIdsAreUnique) {
    portfolioRepo->saveSnapshot({"pf", finance::Currency::USD, 0, "pf1", {}, {{finance::Currency::USD, 50000.0}}});

    finance::Transaction tx{"",
                            kDay,
                            finance::TransactionType::Deposit,
                            finance::AssetType::Cash,
                            "USD",
                            1000.0,
                            1.0,
                            0.0,
                            finance::Currency::USD};
    std::string id1 = service->addTransaction("pf1", tx);
    std::string id2 = service->addTransaction("pf1", tx);

    EXPECT_NE(id1, id2);
}

// ============================================================
// deleteTransaction
// ============================================================

TEST_F(PortfolioServiceTest, DeleteTransactionRemovesIt) {
    portfolioRepo->saveSnapshot({"pf", finance::Currency::USD, 0, "pf1", {}, {{finance::Currency::USD, 10000.0}}});

    std::string id = service->addTransaction("pf1",
                                             finance::Transaction{"",
                                                                  kDay,
                                                                  finance::TransactionType::Deposit,
                                                                  finance::AssetType::Cash,
                                                                  "USD",
                                                                  5000.0,
                                                                  1.0,
                                                                  0.0,
                                                                  finance::Currency::USD});

    service->deleteTransaction("pf1", id);

    EXPECT_TRUE(service->listTransactions("pf1", 0).empty());
}

TEST_F(PortfolioServiceTest, DeleteTransactionThrowsWhenIdNotFound) {
    portfolioRepo->saveSnapshot({"pf", finance::Currency::USD, 0, "pf1", {}, {}});
    EXPECT_THROW(service->deleteTransaction("pf1", "nonexistent"), std::runtime_error);
}

// ============================================================
// updateTransaction
// ============================================================

TEST_F(PortfolioServiceTest, UpdateTransactionReplacesWithNewId) {
    portfolioRepo->saveSnapshot({"pf", finance::Currency::USD, 0, "pf1", {}, {{finance::Currency::USD, 50000.0}}});

    std::string oldId = service->addTransaction("pf1",
                                                finance::Transaction{"",
                                                                     kDay,
                                                                     finance::TransactionType::Deposit,
                                                                     finance::AssetType::Cash,
                                                                     "USD",
                                                                     5000.0,
                                                                     1.0,
                                                                     0.0,
                                                                     finance::Currency::USD});

    finance::Transaction corrected{"",
                                   kDay,
                                   finance::TransactionType::Deposit,
                                   finance::AssetType::Cash,
                                   "USD",
                                   7000.0,
                                   1.0,
                                   0.0,
                                   finance::Currency::USD};
    std::string newId = service->updateTransaction("pf1", oldId, corrected);

    EXPECT_NE(newId, oldId);
    EXPECT_FALSE(newId.empty());

    auto txns = service->listTransactions("pf1", 0);
    ASSERT_EQ(txns.size(), 1);
    EXPECT_EQ(txns[0].id, newId);
    EXPECT_NEAR(txns[0].quantity, 7000.0, 1e-9);
}

// ============================================================
// createNew
// ============================================================

TEST_F(PortfolioServiceTest, CreateNewReturnsCorrectMetadata) {
    finance::Portfolio p = service->createNew("new_pf", "My Fund", finance::Currency::EUR);
    EXPECT_EQ(p.id(), "new_pf");
    EXPECT_EQ(p.name(), "My Fund");
    EXPECT_EQ(p.baseCurrency(), finance::Currency::EUR);
    EXPECT_TRUE(p.positions().empty());
}

TEST_F(PortfolioServiceTest, CreateNewPersistsSentinelSnapshot) {
    service->createNew("new_pf", "My Fund", finance::Currency::EUR, 1 * kDay);
    auto snap = portfolioRepo->loadLatestSnapshot("new_pf");
    ASSERT_TRUE(snap.has_value());
    EXPECT_EQ(snap->timestampMs, 1 * kDay - 1);
    EXPECT_EQ(snap->baseCurrency, finance::Currency::EUR);
}

TEST_F(PortfolioServiceTest, CreateNewAllowsSubsequentLoad) {
    service->createNew("new_pf", "My Fund", finance::Currency::EUR);
    finance::Portfolio p = service->load("new_pf");
    EXPECT_EQ(p.id(), "new_pf");
    EXPECT_EQ(p.name(), "My Fund");
}

TEST_F(PortfolioServiceTest, CreateNewThrowsWhenAlreadyExists) {
    service->createNew("new_pf", "My Fund", finance::Currency::EUR);
    EXPECT_THROW(service->createNew("new_pf", "Duplicate", finance::Currency::USD), std::runtime_error);
}

// ============================================================
// importTransactions
// ============================================================

TEST_F(PortfolioServiceTest, ImportTransactionsAssignsNonEmptyUniqueIds) {
    service->createNew("imp_pf", "Import Fund", finance::Currency::USD);
    std::vector<finance::Transaction> txns = {
        {"",
         1 * kDay,
         finance::TransactionType::Deposit,
         finance::AssetType::Cash,
         "USD",
         5000.0,
         1.0,
         0.0,
         finance::Currency::USD},
        {"",
         2 * kDay,
         finance::TransactionType::Deposit,
         finance::AssetType::Cash,
         "USD",
         3000.0,
         1.0,
         0.0,
         finance::Currency::USD},
    };
    auto ids = service->importTransactions("imp_pf", txns);

    ASSERT_EQ(ids.size(), 2);
    EXPECT_FALSE(ids[0].empty());
    EXPECT_FALSE(ids[1].empty());
    EXPECT_NE(ids[0], ids[1]);
}

TEST_F(PortfolioServiceTest, ImportTransactionsPersistsAll) {
    service->createNew("imp_pf", "Import Fund", finance::Currency::USD);
    std::vector<finance::Transaction> txns = {
        {"",
         1 * kDay,
         finance::TransactionType::Deposit,
         finance::AssetType::Cash,
         "USD",
         5000.0,
         1.0,
         0.0,
         finance::Currency::USD},
        {"",
         2 * kDay,
         finance::TransactionType::Deposit,
         finance::AssetType::Cash,
         "USD",
         3000.0,
         1.0,
         0.0,
         finance::Currency::USD},
    };
    service->importTransactions("imp_pf", txns);

    auto persisted = service->listTransactions("imp_pf", 0);
    ASSERT_EQ(persisted.size(), 2);
}

TEST_F(PortfolioServiceTest, ImportTransactionsBuildsSnapshotChain) {
    registerEquity("AAPL", finance::Currency::USD, 100.0);
    service->createNew("imp_pf", "Import Fund", finance::Currency::USD);
    std::vector<finance::Transaction> txns = {
        {"",
         1 * kDay,
         finance::TransactionType::Deposit,
         finance::AssetType::Cash,
         "USD",
         1000.0,
         1.0,
         0.0,
         finance::Currency::USD},
        {"",
         2 * kDay,
         finance::TransactionType::Buy,
         finance::AssetType::Equity,
         "AAPL",
         5.0,
         100.0,
         0.0,
         finance::Currency::USD},
    };
    service->importTransactions("imp_pf", txns);

    // At constant price a Deposit then Buy is value-neutral. Values per tick:
    //   0: sentinel snapshot (empty) → 0
    //   1: after Deposit 1000       → 1000
    //   2: after Buy 5×100          → 5×100 + 500 = 1000
    //   3: (no change)              → 1000
    TimeSeries series = service->valueSeries("imp_pf", 0, 3 * kDay, kDay);
    ASSERT_EQ(series.size(), 4);
    EXPECT_DOUBLE_EQ(series.getValues()[0], 0.0);
    EXPECT_DOUBLE_EQ(series.getValues()[1], 1000.0);
    EXPECT_DOUBLE_EQ(series.getValues()[2], 1000.0);
    EXPECT_DOUBLE_EQ(series.getValues()[3], 1000.0);
}

// ============================================================
// deletePortfolio
// ============================================================

TEST_F(PortfolioServiceTest, DeletePortfolioRemovesFromList) {
    portfolioRepo->saveSnapshot({"pf", finance::Currency::USD, 0, "del_pf", {}, {}});

    service->deletePortfolio("del_pf");

    auto ids = service->listPortfolioIds();
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), "del_pf") == ids.end());
}

TEST_F(PortfolioServiceTest, DeletePortfolioMakesLoadThrow) {
    portfolioRepo->saveSnapshot({"pf", finance::Currency::USD, 0, "del_pf", {}, {}});
    service->deletePortfolio("del_pf");
    EXPECT_THROW(service->load("del_pf"), std::runtime_error);
}

TEST_F(PortfolioServiceTest, DeletePortfolioThrowsWhenNotFound) {
    EXPECT_THROW(service->deletePortfolio("nonexistent"), std::runtime_error);
}

// ============================================================
// loadMetadata
// ============================================================

TEST_F(PortfolioServiceTest, LoadMetadataReturnsCorrectFields) {
    portfolioRepo->saveSnapshot({"My Fund", finance::Currency::EUR, 0, "meta_pf", {}, {}});
    auto meta = service->loadMetadata("meta_pf");
    EXPECT_EQ(meta.id, "meta_pf");
    EXPECT_EQ(meta.name, "My Fund");
    EXPECT_EQ(meta.baseCurrency, finance::Currency::EUR);
}

TEST_F(PortfolioServiceTest, LoadMetadataThrowsWhenMissing) {
    EXPECT_THROW(service->loadMetadata("nonexistent"), std::runtime_error);
}
