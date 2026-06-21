// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "finapp/data/repository/implementation/CsvRepository/CSVPortfolioRepository.hpp"
#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/common/AssetId.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finapp/finance/portfolio/PortfolioSnapshot.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"

class CSVPortfolioRepositoryTest : public ::testing::Test {
 protected:
    std::filesystem::path testDir;
    std::unique_ptr<finapp::CSVPortfolioRepository> repo;

    void SetUp() override {
        testDir = std::filesystem::temp_directory_path() / "finapp_csv_portfolio_test";
        std::filesystem::remove_all(testDir);
        std::filesystem::create_directories(testDir);
        repo = std::make_unique<finapp::CSVPortfolioRepository>(testDir);
    }

    void TearDown() override { std::filesystem::remove_all(testDir); }

    static std::string nextId() {
        static int n = 0;
        return "test-tx-" + std::to_string(++n);
    }

    static finance::Transaction makeBuy(int64_t ts, const std::string& ticker, double qty, double price,
                                        double fees = 0.0) {
        return finance::Transaction{nextId(),
                                    ts,
                                    finance::TransactionType::Buy,
                                    finance::AssetType::Equity,
                                    ticker,
                                    qty,
                                    price,
                                    fees,
                                    finance::Currency::USD};
    }

    static finance::Transaction makeDeposit(int64_t ts, double qty,
                                            finance::Currency currency = finance::Currency::USD) {
        return finance::Transaction{nextId(),
                                    ts,
                                    finance::TransactionType::Deposit,
                                    finance::AssetType::Cash,
                                    toString(currency),
                                    qty,
                                    1.0,
                                    0.0,
                                    currency};
    }

    static finance::PortfolioSnapshot makeSnapshot(const std::string& portfolioId, int64_t ts,
                                                   const std::vector<finance::SnapshotPosition>& positions,
                                                   const std::unordered_map<finance::Currency, double>& cash,
                                                   const std::string& name = "Test Portfolio",
                                                   finance::Currency baseCurrency = finance::Currency::USD) {
        return finance::PortfolioSnapshot{name, baseCurrency, ts, portfolioId, positions, cash};
    }
};

// ============================================================
// Transactions: append + load roundtrip
// ============================================================

TEST_F(CSVPortfolioRepositoryTest, AppendAndLoadTransactions) {
    std::vector<finance::Transaction> txns = {makeBuy(1000, "AAPL", 10.0, 150.0), makeBuy(2000, "MSFT", 5.0, 300.0)};

    repo->appendTransactions("pf1", txns);
    auto loaded = repo->loadTransactions("pf1", 0);

    ASSERT_EQ(loaded.size(), 2);
    EXPECT_EQ(loaded[0].id, txns[0].id);
    EXPECT_EQ(loaded[0].timestampsMs, 1000);
    EXPECT_EQ(loaded[0].assetTicker, "AAPL");
    EXPECT_NEAR(loaded[0].quantity, 10.0, 1e-9);
    EXPECT_NEAR(loaded[0].pricePerUnit, 150.0, 1e-9);
    EXPECT_EQ(loaded[1].id, txns[1].id);
    EXPECT_EQ(loaded[1].timestampsMs, 2000);
    EXPECT_EQ(loaded[1].assetTicker, "MSFT");
}

TEST_F(CSVPortfolioRepositoryTest, AppendTransactionsPreservesAllFields) {
    finance::Transaction txn{"preserve-id-1",
                             1000,
                             finance::TransactionType::Sell,
                             finance::AssetType::Equity,
                             "GOOG",
                             3.5,
                             2800.0,
                             9.99,
                             finance::Currency::EUR};
    repo->appendTransactions("pf1", {txn});

    auto loaded = repo->loadTransactions("pf1", 0);
    ASSERT_EQ(loaded.size(), 1);
    EXPECT_EQ(loaded[0].id, "preserve-id-1");
    EXPECT_EQ(loaded[0].type, finance::TransactionType::Sell);
    EXPECT_EQ(loaded[0].assetType, finance::AssetType::Equity);
    EXPECT_EQ(loaded[0].assetTicker, "GOOG");
    EXPECT_NEAR(loaded[0].quantity, 3.5, 1e-9);
    EXPECT_NEAR(loaded[0].pricePerUnit, 2800.0, 1e-9);
    EXPECT_NEAR(loaded[0].fees, 9.99, 1e-9);
    EXPECT_EQ(loaded[0].settlementCurrency, finance::Currency::EUR);
}

TEST_F(CSVPortfolioRepositoryTest, AppendTransactionsDeduplicates) {
    // Deduplication is id-based: appending the same id twice must not create a duplicate.
    finance::Transaction tx{"dedup-id-1",
                            1000,
                            finance::TransactionType::Buy,
                            finance::AssetType::Equity,
                            "AAPL",
                            10.0,
                            150.0,
                            0.0,
                            finance::Currency::USD};
    repo->appendTransactions("pf1", {tx});

    finance::Transaction newTx = makeBuy(2000, "MSFT", 5.0, 300.0);
    repo->appendTransactions("pf1", {tx, newTx});  // tx has same id — should be ignored

    auto loaded = repo->loadTransactions("pf1", 0);
    ASSERT_EQ(loaded.size(), 2);
}

TEST_F(CSVPortfolioRepositoryTest, AppendMultipleBatches) {
    repo->appendTransactions("pf1", {makeBuy(1000, "AAPL", 10.0, 150.0)});
    repo->appendTransactions("pf1", {makeBuy(2000, "MSFT", 5.0, 300.0)});
    repo->appendTransactions("pf1", {makeDeposit(3000, 10000.0)});

    auto loaded = repo->loadTransactions("pf1", 0);
    ASSERT_EQ(loaded.size(), 3);
}

// ============================================================
// Transactions: load with afterTimestamp filter
// ============================================================

TEST_F(CSVPortfolioRepositoryTest, LoadTransactionsFiltersByTimestamp) {
    std::vector<finance::Transaction> txns = {
        makeBuy(1000, "AAPL", 10.0, 150.0), makeBuy(2000, "MSFT", 5.0, 300.0), makeBuy(3000, "GOOG", 2.0, 2800.0)};
    repo->appendTransactions("pf1", txns);

    auto loaded = repo->loadTransactions("pf1", 2000);
    ASSERT_EQ(loaded.size(), 2);
    EXPECT_EQ(loaded[0].timestampsMs, 2000);
    EXPECT_EQ(loaded[1].timestampsMs, 3000);
}

TEST_F(CSVPortfolioRepositoryTest, LoadTransactionsAfterAllReturnsEmpty) {
    repo->appendTransactions("pf1", {makeBuy(1000, "AAPL", 10.0, 150.0)});

    auto loaded = repo->loadTransactions("pf1", 9999);
    EXPECT_TRUE(loaded.empty());
}

TEST_F(CSVPortfolioRepositoryTest, LoadTransactionsThrowsWhenFileMissing) {
    EXPECT_THROW(repo->loadTransactions("nonexistent", 0), std::runtime_error);
}

// ============================================================
// Snapshots: save + load roundtrip
// ============================================================

TEST_F(CSVPortfolioRepositoryTest, SaveAndLoadSnapshotWithPositionsAndCash) {
    std::vector<finance::SnapshotPosition> positions = {
        finance::SnapshotPosition{finance::AssetId{finance::AssetType::Equity, "AAPL"}, 10.0},
        finance::SnapshotPosition{finance::AssetId{finance::AssetType::Equity, "MSFT"}, 5.0},
    };
    std::unordered_map<finance::Currency, double> cash = {{finance::Currency::USD, 50000.0},
                                                          {finance::Currency::EUR, 10000.0}};

    auto snapshot = makeSnapshot("pf1", 1000, positions, cash, "My Portfolio", finance::Currency::EUR);
    repo->saveSnapshot(snapshot);

    auto loaded = repo->loadLatestSnapshot("pf1");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->timestampMs, 1000);
    EXPECT_EQ(loaded->portfolioId, "pf1");
    EXPECT_EQ(loaded->name, "My Portfolio");
    EXPECT_EQ(loaded->baseCurrency, finance::Currency::EUR);
    ASSERT_EQ(loaded->positions.size(), 2);
    ASSERT_EQ(loaded->cashBalances.size(), 2);
    EXPECT_NEAR(loaded->cashBalances.at(finance::Currency::USD), 50000.0, 1e-6);
    EXPECT_NEAR(loaded->cashBalances.at(finance::Currency::EUR), 10000.0, 1e-6);
}

TEST_F(CSVPortfolioRepositoryTest, SaveAndLoadSnapshotPositionsRoundtrip) {
    std::vector<finance::SnapshotPosition> positions = {
        finance::SnapshotPosition{finance::AssetId{finance::AssetType::Equity, "AAPL"}, 10.5},
        finance::SnapshotPosition{finance::AssetId{finance::AssetType::Cash, "USD"}, 1000.0},
    };

    auto snapshot = makeSnapshot("pf1", 5000, positions, {});
    repo->saveSnapshot(snapshot);

    auto loaded = repo->loadLatestSnapshot("pf1");
    ASSERT_TRUE(loaded.has_value());
    ASSERT_EQ(loaded->positions.size(), 2);

    // Find AAPL position
    bool foundAAPL = false;
    for (const auto& pos : loaded->positions) {
        if (pos.assetId.ticker == "AAPL") {
            EXPECT_EQ(pos.assetId.type, finance::AssetType::Equity);
            EXPECT_NEAR(pos.quantity, 10.5, 1e-9);
            foundAAPL = true;
        }
    }
    EXPECT_TRUE(foundAAPL);
}

TEST_F(CSVPortfolioRepositoryTest, LoadLatestSnapshotReturnsNewest) {
    auto snap1 = makeSnapshot("pf1",
                              1000,
                              {finance::SnapshotPosition{finance::AssetId{finance::AssetType::Equity, "AAPL"}, 10.0}},
                              {{finance::Currency::USD, 5000.0}});
    auto snap2 = makeSnapshot("pf1",
                              2000,
                              {finance::SnapshotPosition{finance::AssetId{finance::AssetType::Equity, "AAPL"}, 20.0}},
                              {{finance::Currency::USD, 3000.0}});

    repo->saveSnapshot(snap1);
    repo->saveSnapshot(snap2);

    auto loaded = repo->loadLatestSnapshot("pf1");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->timestampMs, 2000);
    ASSERT_EQ(loaded->positions.size(), 1);
    EXPECT_NEAR(loaded->positions[0].quantity, 20.0, 1e-9);
    EXPECT_NEAR(loaded->cashBalances.at(finance::Currency::USD), 3000.0, 1e-6);
}

TEST_F(CSVPortfolioRepositoryTest, LoadLatestSnapshotReturnsNulloptWhenMissing) {
    auto loaded = repo->loadLatestSnapshot("nonexistent");
    EXPECT_FALSE(loaded.has_value());
}

TEST_F(CSVPortfolioRepositoryTest, SaveSnapshotWithEmptyPositions) {
    auto snapshot = makeSnapshot("pf1", 1000, {}, {{finance::Currency::USD, 50000.0}});
    repo->saveSnapshot(snapshot);

    auto loaded = repo->loadLatestSnapshot("pf1");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_TRUE(loaded->positions.empty());
    EXPECT_NEAR(loaded->cashBalances.at(finance::Currency::USD), 50000.0, 1e-6);
}

TEST_F(CSVPortfolioRepositoryTest, SaveSnapshotWithEmptyCash) {
    auto snapshot = makeSnapshot(
        "pf1", 1000, {finance::SnapshotPosition{finance::AssetId{finance::AssetType::Equity, "AAPL"}, 10.0}}, {});
    repo->saveSnapshot(snapshot);

    auto loaded = repo->loadLatestSnapshot("pf1");
    ASSERT_TRUE(loaded.has_value());
    ASSERT_EQ(loaded->positions.size(), 1);
    EXPECT_TRUE(loaded->cashBalances.empty());
}

// ============================================================
// exists
// ============================================================

TEST_F(CSVPortfolioRepositoryTest, ExistsReturnsFalseWhenMissing) { EXPECT_FALSE(repo->exists("nonexistent")); }

TEST_F(CSVPortfolioRepositoryTest, ExistsReturnsTrueAfterSnapshot) {
    auto snapshot = makeSnapshot("pf1", 1000, {}, {{finance::Currency::USD, 1000.0}});
    repo->saveSnapshot(snapshot);
    EXPECT_TRUE(repo->exists("pf1"));
}

TEST_F(CSVPortfolioRepositoryTest, ExistsReturnsTrueAfterTransactions) {
    repo->appendTransactions("pf2", {makeBuy(1000, "AAPL", 10.0, 150.0)});
    EXPECT_TRUE(repo->exists("pf2"));
}

// ============================================================
// listPortfolioIds
// ============================================================

TEST_F(CSVPortfolioRepositoryTest, ListPortfolioIdsEmpty) {
    std::filesystem::create_directories(testDir / "Portfolio");
    auto ids = repo->listPortfolioIds();
    EXPECT_TRUE(ids.empty());
}

TEST_F(CSVPortfolioRepositoryTest, ListPortfolioIdsReturnsAll) {
    repo->saveSnapshot(makeSnapshot("alpha", 1000, {}, {{finance::Currency::USD, 100.0}}));
    repo->saveSnapshot(makeSnapshot("beta", 2000, {}, {{finance::Currency::EUR, 200.0}}));

    auto ids = repo->listPortfolioIds();
    ASSERT_EQ(ids.size(), 2);
    std::sort(ids.begin(), ids.end());
    EXPECT_EQ(ids[0], "alpha");
    EXPECT_EQ(ids[1], "beta");
}

TEST_F(CSVPortfolioRepositoryTest, ListPortfolioIdsDoesNotIncludeTransactionFiles) {
    repo->saveSnapshot(makeSnapshot("pf1", 1000, {}, {{finance::Currency::USD, 100.0}}));
    repo->appendTransactions("pf1", {makeBuy(1000, "AAPL", 10.0, 150.0)});

    auto ids = repo->listPortfolioIds();
    ASSERT_EQ(ids.size(), 1);
    EXPECT_EQ(ids[0], "pf1");
}

// ============================================================
// Transaction types roundtrip
// ============================================================

TEST_F(CSVPortfolioRepositoryTest, AllTransactionTypesRoundtrip) {
    std::vector<finance::Transaction> txns = {
        {"rt-1",
         1000,
         finance::TransactionType::Buy,
         finance::AssetType::Equity,
         "AAPL",
         10.0,
         150.0,
         5.0,
         finance::Currency::USD},
        {"rt-2",
         2000,
         finance::TransactionType::Sell,
         finance::AssetType::Equity,
         "AAPL",
         5.0,
         160.0,
         5.0,
         finance::Currency::USD},
        {"rt-3",
         3000,
         finance::TransactionType::Deposit,
         finance::AssetType::Cash,
         "USD",
         10000.0,
         1.0,
         0.0,
         finance::Currency::USD},
        {"rt-4",
         4000,
         finance::TransactionType::Withdrawal,
         finance::AssetType::Cash,
         "USD",
         500.0,
         1.0,
         0.0,
         finance::Currency::USD},
        {"rt-5",
         5000,
         finance::TransactionType::Dividend,
         finance::AssetType::Equity,
         "AAPL",
         0.0,
         0.82,
         0.0,
         finance::Currency::USD},
        {"rt-6",
         6000,
         finance::TransactionType::Split,
         finance::AssetType::Equity,
         "AAPL",
         4.0,
         0.0,
         0.0,
         finance::Currency::USD},
    };

    repo->appendTransactions("pf1", txns);
    auto loaded = repo->loadTransactions("pf1", 0);

    ASSERT_EQ(loaded.size(), 6);
    EXPECT_EQ(loaded[0].type, finance::TransactionType::Buy);
    EXPECT_EQ(loaded[1].type, finance::TransactionType::Sell);
    EXPECT_EQ(loaded[2].type, finance::TransactionType::Deposit);
    EXPECT_EQ(loaded[3].type, finance::TransactionType::Withdrawal);
    EXPECT_EQ(loaded[4].type, finance::TransactionType::Dividend);
    EXPECT_EQ(loaded[5].type, finance::TransactionType::Split);
}

// ============================================================
// deleteTransaction
// ============================================================

TEST_F(CSVPortfolioRepositoryTest, DeleteTransactionRemovesFromLog) {
    finance::Transaction tx{"del-1",
                            2000,
                            finance::TransactionType::Buy,
                            finance::AssetType::Equity,
                            "AAPL",
                            10.0,
                            150.0,
                            0.0,
                            finance::Currency::USD};
    repo->appendTransactions("pf1", {tx, makeBuy(3000, "MSFT", 5.0, 300.0)});

    repo->deleteTransaction("pf1", "del-1");

    auto loaded = repo->loadTransactions("pf1", 0);
    ASSERT_EQ(loaded.size(), 1);
    EXPECT_EQ(loaded[0].assetTicker, "MSFT");
}

TEST_F(CSVPortfolioRepositoryTest, DeleteTransactionInvalidatesSubsequentSnapshots) {
    // Snapshot before the transaction.
    repo->saveSnapshot(makeSnapshot("pf1", 1000, {}, {{finance::Currency::USD, 10000.0}}));

    finance::Transaction tx{"cascade-1",
                            2000,
                            finance::TransactionType::Buy,
                            finance::AssetType::Equity,
                            "AAPL",
                            10.0,
                            150.0,
                            0.0,
                            finance::Currency::USD};
    repo->appendTransactions("pf1", {tx});

    // Snapshot after the transaction — must be invalidated by delete.
    repo->saveSnapshot(
        makeSnapshot("pf1",
                     3000,
                     {finance::SnapshotPosition{finance::AssetId{finance::AssetType::Equity, "AAPL"}, 10.0}},
                     {{finance::Currency::USD, 8500.0}}));
    ASSERT_EQ(repo->loadLatestSnapshot("pf1")->timestampMs, 3000);

    repo->deleteTransaction("pf1", "cascade-1");

    // Latest snapshot rolls back to the one before the deleted transaction.
    auto latest = repo->loadLatestSnapshot("pf1");
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->timestampMs, 1000);

    // Transaction log is now empty.
    EXPECT_TRUE(repo->loadTransactions("pf1", 0).empty());
}

TEST_F(CSVPortfolioRepositoryTest, DeleteTransactionThrowsWhenIdNotFound) {
    repo->appendTransactions("pf1", {makeBuy(1000, "AAPL", 10.0, 150.0)});
    EXPECT_THROW(repo->deleteTransaction("pf1", "nonexistent-id"), std::runtime_error);
}

TEST_F(CSVPortfolioRepositoryTest, DeleteTransactionThrowsWhenNoFile) {
    EXPECT_THROW(repo->deleteTransaction("nonexistent", "any-id"), std::runtime_error);
}
