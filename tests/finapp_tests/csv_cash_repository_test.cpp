// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "finapp/data/repository/implementation/CsvRepository/CSVCashRepository.hpp"
#include "finapp/finance/asset/Cash.hpp"
#include "finapp/finance/asset/Equity.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finapp/finance/common/Currency.hpp"

using namespace finance;
using namespace finapp;

class CSVCashRepositoryTest : public ::testing::Test {
 protected:
    std::filesystem::path testDir;
    std::unique_ptr<CSVCashRepository> repo;

    void SetUp() override {
        testDir = std::filesystem::temp_directory_path() / "finapp_csv_cash_test";
        std::filesystem::remove_all(testDir);
        std::filesystem::create_directories(testDir);
        repo = std::make_unique<CSVCashRepository>(testDir);
    }

    void TearDown() override { std::filesystem::remove_all(testDir); }
};

// ============================================================
// save + load roundtrip
// ============================================================

TEST_F(CSVCashRepositoryTest, SaveAndLoadRoundtrip) {
    auto cash = std::make_shared<Cash>(Currency::USD);
    repo->save(cash);

    auto loaded = repo->load(cash->ticker());
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->denomination(), Currency::USD);
    EXPECT_EQ(loaded->type(), AssetType::Cash);
}

TEST_F(CSVCashRepositoryTest, SaveAndLoadMultipleCurrencies) {
    auto usd = std::make_shared<Cash>(Currency::USD);
    auto eur = std::make_shared<Cash>(Currency::EUR);
    auto jpy = std::make_shared<Cash>(Currency::JPY);

    repo->save(usd);
    repo->save(eur);
    repo->save(jpy);

    EXPECT_EQ(repo->load(usd->ticker())->denomination(), Currency::USD);
    EXPECT_EQ(repo->load(eur->ticker())->denomination(), Currency::EUR);
    EXPECT_EQ(repo->load(jpy->ticker())->denomination(), Currency::JPY);
}

// ============================================================
// exists
// ============================================================

TEST_F(CSVCashRepositoryTest, ExistsReturnsFalseWhenMissing) { EXPECT_FALSE(repo->exists("NONEXIST")); }

TEST_F(CSVCashRepositoryTest, ExistsReturnsTrueAfterSave) {
    auto cash = std::make_shared<Cash>(Currency::GBP);
    repo->save(cash);
    EXPECT_TRUE(repo->exists(cash->ticker()));
}

// ============================================================
// load errors
// ============================================================

TEST_F(CSVCashRepositoryTest, LoadThrowsWhenMissing) { EXPECT_THROW(repo->load("NONEXIST"), std::runtime_error); }

// ============================================================
// listTickers
// ============================================================

TEST_F(CSVCashRepositoryTest, ListTickersEmpty) {
    std::filesystem::create_directories(testDir / "Cash");
    auto tickers = repo->listTickers();
    EXPECT_TRUE(tickers.empty());
}

TEST_F(CSVCashRepositoryTest, ListTickersReturnsAllSaved) {
    repo->save(std::make_shared<Cash>(Currency::USD));
    repo->save(std::make_shared<Cash>(Currency::EUR));

    auto tickers = repo->listTickers();
    ASSERT_EQ(tickers.size(), 2);
    std::sort(tickers.begin(), tickers.end());
    EXPECT_EQ(tickers[0], "EUR Cash");
    EXPECT_EQ(tickers[1], "USD Cash");
}

// ============================================================
// loadAll
// ============================================================

TEST_F(CSVCashRepositoryTest, LoadAllReturnsRequestedTickers) {
    auto usd = std::make_shared<Cash>(Currency::USD);
    auto eur = std::make_shared<Cash>(Currency::EUR);
    auto gbp = std::make_shared<Cash>(Currency::GBP);
    repo->save(usd);
    repo->save(eur);
    repo->save(gbp);

    auto all = repo->loadAll({usd->ticker(), gbp->ticker()});
    ASSERT_EQ(all.size(), 2);
    EXPECT_NE(all.find(usd->ticker()), all.end());
    EXPECT_NE(all.find(gbp->ticker()), all.end());
}

// ============================================================
// save type safety
// ============================================================

TEST_F(CSVCashRepositoryTest, SaveThrowsForNonCashAsset) {
    auto equity = std::make_shared<Equity>("AAPL", "Apple", Currency::USD, "NASDAQ", "Tech");
    EXPECT_THROW(repo->save(equity), std::runtime_error);
}
