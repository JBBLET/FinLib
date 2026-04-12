// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

#include "finapp/data/repository/implementation/CsvRepository/CSVFXRepository.hpp"
#include "finapp/data/repository/interface/IFXRepository.hpp"
#include "finapp/finance/common/Currency.hpp"

using namespace finance;

class CSVFXRepositoryTest : public ::testing::Test {
 protected:
    std::filesystem::path testDir;
    std::unique_ptr<CSVFXRepository> repo;

    void SetUp() override {
        testDir = std::filesystem::temp_directory_path() / "finapp_csv_fx_test";
        std::filesystem::remove_all(testDir);
        std::filesystem::create_directories(testDir);
        repo = std::make_unique<CSVFXRepository>(testDir);
    }

    void TearDown() override { std::filesystem::remove_all(testDir); }
};

// ============================================================
// save + load roundtrip
// ============================================================

TEST_F(CSVFXRepositoryTest, SaveAndLoadRoundtrip) {
    FXInfos info{Currency::EUR, Currency::USD, "EURUSD"};
    repo->save(info);

    auto loaded = repo->load(Currency::EUR, Currency::USD);
    EXPECT_EQ(loaded.baseCurrency, Currency::EUR);
    EXPECT_EQ(loaded.quoteCurrency, Currency::USD);
    EXPECT_EQ(loaded.timeseriesID, "EURUSD");
}

TEST_F(CSVFXRepositoryTest, SaveMultiplePairs) {
    repo->save(FXInfos{Currency::EUR, Currency::USD, "EURUSD"});
    repo->save(FXInfos{Currency::GBP, Currency::JPY, "GBPJPY"});
    repo->save(FXInfos{Currency::CAD, Currency::KRW, "CADKRW"});

    EXPECT_EQ(repo->load(Currency::EUR, Currency::USD).timeseriesID, "EURUSD");
    EXPECT_EQ(repo->load(Currency::GBP, Currency::JPY).timeseriesID, "GBPJPY");
    EXPECT_EQ(repo->load(Currency::CAD, Currency::KRW).timeseriesID, "CADKRW");
}

TEST_F(CSVFXRepositoryTest, SaveUpdatesExistingPair) {
    repo->save(FXInfos{Currency::EUR, Currency::USD, "EURUSD_v1"});
    repo->save(FXInfos{Currency::EUR, Currency::USD, "EURUSD_v2"});

    auto loaded = repo->load(Currency::EUR, Currency::USD);
    EXPECT_EQ(loaded.timeseriesID, "EURUSD_v2");
}

TEST_F(CSVFXRepositoryTest, SaveUpdateDoesNotDuplicate) {
    repo->save(FXInfos{Currency::EUR, Currency::USD, "EURUSD"});
    repo->save(FXInfos{Currency::GBP, Currency::JPY, "GBPJPY"});
    repo->save(FXInfos{Currency::EUR, Currency::USD, "EURUSD_updated"});

    // Load both to verify the GBP/JPY pair wasn't lost
    EXPECT_EQ(repo->load(Currency::EUR, Currency::USD).timeseriesID, "EURUSD_updated");
    EXPECT_EQ(repo->load(Currency::GBP, Currency::JPY).timeseriesID, "GBPJPY");
}

// ============================================================
// exists
// ============================================================

TEST_F(CSVFXRepositoryTest, ExistsReturnsFalseWhenMissing) {
    EXPECT_FALSE(repo->exists(Currency::EUR, Currency::USD));
}

TEST_F(CSVFXRepositoryTest, ExistsReturnsTrueAfterSave) {
    repo->save(FXInfos{Currency::EUR, Currency::USD, "EURUSD"});
    EXPECT_TRUE(repo->exists(Currency::EUR, Currency::USD));
}

TEST_F(CSVFXRepositoryTest, ExistsDistinguishesDirection) {
    repo->save(FXInfos{Currency::EUR, Currency::USD, "EURUSD"});
    EXPECT_TRUE(repo->exists(Currency::EUR, Currency::USD));
    EXPECT_FALSE(repo->exists(Currency::USD, Currency::EUR));
}

// ============================================================
// load errors
// ============================================================

TEST_F(CSVFXRepositoryTest, LoadThrowsWhenPairMissing) {
    EXPECT_THROW(repo->load(Currency::EUR, Currency::USD), std::runtime_error);
}

TEST_F(CSVFXRepositoryTest, LoadThrowsWhenFileExistsButPairMissing) {
    repo->save(FXInfos{Currency::EUR, Currency::USD, "EURUSD"});
    EXPECT_THROW(repo->load(Currency::GBP, Currency::JPY), std::runtime_error);
}

// ============================================================
// empty file handling
// ============================================================

TEST_F(CSVFXRepositoryTest, ExistsReturnsFalseWhenNoFileExists) {
    EXPECT_FALSE(repo->exists(Currency::USD, Currency::JPY));
}
