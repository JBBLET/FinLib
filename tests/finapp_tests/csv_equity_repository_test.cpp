// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "finapp/data/repository/implementation/CsvRepository/CSVEquityRepository.hpp"
#include "finapp/finance/asset/Cash.hpp"
#include "finapp/finance/asset/Equity.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finapp/finance/common/Currency.hpp"

using namespace finance;
using namespace finapp;

class CSVEquityRepositoryTest : public ::testing::Test {
 protected:
    std::filesystem::path testDir;
    std::unique_ptr<CSVEquityRepository> repo;

    void SetUp() override {
        testDir = std::filesystem::temp_directory_path() / "finapp_csv_equity_test";
        std::filesystem::remove_all(testDir);
        std::filesystem::create_directories(testDir);
        repo = std::make_unique<CSVEquityRepository>(testDir);
    }

    void TearDown() override { std::filesystem::remove_all(testDir); }

    static std::shared_ptr<const IAsset> makeEquity(const std::string& ticker, const std::string& name, Currency denom,
                                                    const std::string& exchange, const std::string& sector) {
        return std::make_shared<Equity>(ticker, name, denom, exchange, sector);
    }
};

// ============================================================
// save + load roundtrip
// ============================================================

TEST_F(CSVEquityRepositoryTest, SaveAndLoadRoundtrip) {
    auto equity = makeEquity("AAPL", "Apple Inc", Currency::USD, "NASDAQ", "Technology");
    repo->save(equity);

    auto loaded = repo->load("AAPL");
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->ticker(), "AAPL");
    EXPECT_EQ(loaded->name(), "Apple Inc");
    EXPECT_EQ(loaded->denomination(), Currency::USD);
    EXPECT_EQ(loaded->type(), AssetType::Equity);

    auto equityLoaded = std::dynamic_pointer_cast<const Equity>(loaded);
    ASSERT_NE(equityLoaded, nullptr);
    EXPECT_EQ(equityLoaded->exchange(), "NASDAQ");
    EXPECT_EQ(equityLoaded->sector(), "Technology");
}

TEST_F(CSVEquityRepositoryTest, SaveOverwritesExisting) {
    auto equity1 = makeEquity("AAPL", "Apple Inc", Currency::USD, "NASDAQ", "Technology");
    repo->save(equity1);

    auto equity2 = makeEquity("AAPL", "Apple Updated", Currency::EUR, "NYSE", "Consumer");
    repo->save(equity2);

    auto loaded = std::dynamic_pointer_cast<const Equity>(repo->load("AAPL"));
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->name(), "Apple Updated");
    EXPECT_EQ(loaded->denomination(), Currency::EUR);
    EXPECT_EQ(loaded->exchange(), "NYSE");
}

// ============================================================
// exists
// ============================================================

TEST_F(CSVEquityRepositoryTest, ExistsReturnsFalseWhenMissing) { EXPECT_FALSE(repo->exists("NONEXIST")); }

TEST_F(CSVEquityRepositoryTest, ExistsReturnsTrueAfterSave) {
    repo->save(makeEquity("MSFT", "Microsoft", Currency::USD, "NASDAQ", "Technology"));
    EXPECT_TRUE(repo->exists("MSFT"));
}

// ============================================================
// load errors
// ============================================================

TEST_F(CSVEquityRepositoryTest, LoadThrowsWhenMissing) { EXPECT_THROW(repo->load("NONEXIST"), std::runtime_error); }

// ============================================================
// listTickers
// ============================================================

TEST_F(CSVEquityRepositoryTest, ListTickersEmpty) {
    std::filesystem::create_directories(testDir / "Equity");
    auto tickers = repo->listTickers();
    EXPECT_TRUE(tickers.empty());
}

TEST_F(CSVEquityRepositoryTest, ListTickersReturnsAllSaved) {
    repo->save(makeEquity("AAPL", "Apple", Currency::USD, "NASDAQ", "Tech"));
    repo->save(makeEquity("MSFT", "Microsoft", Currency::USD, "NASDAQ", "Tech"));
    repo->save(makeEquity("GOOG", "Alphabet", Currency::USD, "NASDAQ", "Tech"));

    auto tickers = repo->listTickers();
    ASSERT_EQ(tickers.size(), 3);
    std::sort(tickers.begin(), tickers.end());
    EXPECT_EQ(tickers[0], "AAPL");
    EXPECT_EQ(tickers[1], "GOOG");
    EXPECT_EQ(tickers[2], "MSFT");
}

// ============================================================
// loadAll
// ============================================================

TEST_F(CSVEquityRepositoryTest, LoadAllReturnsRequestedTickers) {
    repo->save(makeEquity("AAPL", "Apple", Currency::USD, "NASDAQ", "Tech"));
    repo->save(makeEquity("MSFT", "Microsoft", Currency::USD, "NASDAQ", "Tech"));
    repo->save(makeEquity("GOOG", "Alphabet", Currency::USD, "NASDAQ", "Tech"));

    auto all = repo->loadAll({"AAPL", "GOOG"});
    ASSERT_EQ(all.size(), 2);
    EXPECT_NE(all.find("AAPL"), all.end());
    EXPECT_NE(all.find("GOOG"), all.end());
    EXPECT_EQ(all.find("MSFT"), all.end());
}

// ============================================================
// attributes
// ============================================================

TEST_F(CSVEquityRepositoryTest, SaveAndLoadWithAttributes) {
    auto equity = std::make_shared<Equity>("AAPL", "Apple", Currency::USD, "NASDAQ", "Tech");
    equity->setAttribute("marketcap_series", "AAPL_marketcap");
    equity->setAttribute("divyield_series", "AAPL_divyield");
    repo->save(equity);

    auto loaded = std::dynamic_pointer_cast<const Equity>(repo->load("AAPL"));
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->attributes().size(), 2);
    EXPECT_EQ(loaded->attribute("marketcap_series"), "AAPL_marketcap");
    EXPECT_EQ(loaded->attribute("divyield_series"), "AAPL_divyield");
}

TEST_F(CSVEquityRepositoryTest, SaveAndLoadWithNoAttributes) {
    repo->save(makeEquity("AAPL", "Apple", Currency::USD, "NASDAQ", "Tech"));

    auto loaded = std::dynamic_pointer_cast<const Equity>(repo->load("AAPL"));
    ASSERT_NE(loaded, nullptr);
    EXPECT_TRUE(loaded->attributes().empty());
}

// ============================================================
// save type safety
// ============================================================

TEST_F(CSVEquityRepositoryTest, SaveThrowsForNonEquityAsset) {
    auto cash = std::make_shared<Cash>(Currency::USD);
    EXPECT_THROW(repo->save(cash), std::runtime_error);
}
