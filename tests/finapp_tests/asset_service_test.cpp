// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/asset/Cash.hpp"
#include "finapp/finance/asset/Equity.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finapp/finance/common/AssetId.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finapp/service/AssetService.hpp"
#include "finlib/common/utils/TimeSeriesUtils.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/implementation/CachedTimeSeriesRepository.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"
#include "support/service_test_fakes.hpp"

using namespace finance;
using namespace finance::test;

namespace {

constexpr int64_t kDay = 86'400'000;

class AssetServiceTest : public ::testing::Test {
 protected:
    std::shared_ptr<InMemoryTimeSeriesRepository> innerRepo;
    std::shared_ptr<CachedTimeSeriesRepository> cachedRepo;
    std::shared_ptr<FakeTimeSeriesLoader> provider;
    std::shared_ptr<TimeSeriesService> tsService;
    std::shared_ptr<InMemoryAssetRepository> equityRepo;
    std::shared_ptr<FakeAssetProvider> equityProvider;
    std::unique_ptr<AssetService> service;

    void SetUp() override {
        innerRepo = std::make_shared<InMemoryTimeSeriesRepository>();
        cachedRepo = std::make_shared<CachedTimeSeriesRepository>(innerRepo);
        provider = std::make_shared<FakeTimeSeriesLoader>();
        tsService = std::make_shared<TimeSeriesService>(cachedRepo, provider);
        equityRepo = std::make_shared<InMemoryAssetRepository>();
        equityProvider = std::make_shared<FakeAssetProvider>();

        std::unordered_map<AssetType, std::shared_ptr<IAssetRepository>> repos = {{AssetType::Equity, equityRepo}};
        std::unordered_map<AssetType, std::shared_ptr<IAssetProvider>> providers = {
            {AssetType::Equity, equityProvider}};
        service = std::make_unique<AssetService>(tsService, std::move(repos), std::move(providers));
    }
};

}  // namespace

TEST_F(AssetServiceTest, SaveStoresAssetInRepository) {
    auto aapl = std::make_shared<Equity>("AAPL", "Apple Inc.", Currency::USD);
    service->save(aapl);
    ASSERT_TRUE(equityRepo->exists("AAPL"));
    auto loaded = equityRepo->load("AAPL");
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->ticker(), "AAPL");
}

TEST_F(AssetServiceTest, SaveNullThrows) {
    EXPECT_THROW(service->save(nullptr), std::invalid_argument);
}

TEST_F(AssetServiceTest, LoadHitsRepositoryWhenAvailable) {
    auto msft = std::make_shared<Equity>("MSFT", "Microsoft", Currency::USD);
    equityRepo->save(msft);

    auto loaded = service->load(AssetId{AssetType::Equity, "MSFT"});
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->ticker(), "MSFT");
    EXPECT_EQ(loaded->denomination(), Currency::USD);
}

TEST_F(AssetServiceTest, LoadFallsBackToProviderAndPersists) {
    auto goog = std::make_shared<Equity>("GOOG", "Alphabet", Currency::USD);
    equityProvider->setAsset("GOOG", goog);

    auto loaded = service->load(AssetId{AssetType::Equity, "GOOG"});
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->ticker(), "GOOG");

    // Provider fallback must persist back into the repository.
    EXPECT_TRUE(equityRepo->exists("GOOG"));
}

TEST_F(AssetServiceTest, LoadThrowsWhenAssetMissing) {
    EXPECT_THROW(service->load(AssetId{AssetType::Equity, "NOPE"}), std::runtime_error);
}

TEST_F(AssetServiceTest, LoadCashSynthesizesAsset) {
    auto loaded = service->load(AssetId{AssetType::Cash, "USD"});
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->type(), AssetType::Cash);
    EXPECT_EQ(loaded->denomination(), Currency::USD);
}

TEST_F(AssetServiceTest, LoadCashSynthesisIsCached) {
    auto first = service->load(AssetId{AssetType::Cash, "EUR"});
    auto second = service->load(AssetId{AssetType::Cash, "EUR"});
    EXPECT_EQ(first.get(), second.get());
}

TEST_F(AssetServiceTest, LoadTimeSeriesValueForEquityRangeOverload) {
    auto aapl = std::make_shared<Equity>("AAPL", "Apple Inc.", Currency::USD);
    equityRepo->save(aapl);
    provider->setSeries("AAPL", makeFlatSeries("AAPL", 0, 4 * kDay, kDay, 175.0));

    TimeSeries series = service->loadTimeSeriesValue(AssetId{AssetType::Equity, "AAPL"}, 0, 4 * kDay, kDay);
    ASSERT_EQ(series.size(), 5);
    for (double v : series.getValues()) {
        EXPECT_DOUBLE_EQ(v, 175.0);
    }
}

TEST_F(AssetServiceTest, LoadTimeSeriesValueForCashIsConstantOne) {
    TimeSeries series = service->loadTimeSeriesValue(AssetId{AssetType::Cash, "USD"}, 0, 4 * kDay, kDay);
    ASSERT_EQ(series.size(), 5);
    for (double v : series.getValues()) {
        EXPECT_DOUBLE_EQ(v, 1.0);
    }
}

TEST_F(AssetServiceTest, LoadTimeSeriesValueSharedTimestampsForEquity) {
    auto aapl = std::make_shared<Equity>("AAPL", "Apple Inc.", Currency::USD);
    equityRepo->save(aapl);
    provider->setSeries("AAPL", makeFlatSeries("AAPL", 0, 6 * kDay, kDay, 175.0));

    auto timestamps = common::utils::timeSeries::makeRegularTimestamps(0, 3 * kDay, kDay);
    TimeSeries series = service->loadTimeSeriesValue(AssetId{AssetType::Equity, "AAPL"}, timestamps);
    ASSERT_EQ(series.size(), 4);
    EXPECT_EQ(series.getSharedTimestamps().get(), timestamps.get());
}

TEST_F(AssetServiceTest, LoadTimeSeriesValueSharedTimestampsForCash) {
    auto timestamps = common::utils::timeSeries::makeRegularTimestamps(0, 3 * kDay, kDay);
    TimeSeries series = service->loadTimeSeriesValue(AssetId{AssetType::Cash, "USD"}, timestamps);
    ASSERT_EQ(series.size(), 4);
    EXPECT_EQ(series.getSharedTimestamps().get(), timestamps.get());
    for (double v : series.getValues()) {
        EXPECT_DOUBLE_EQ(v, 1.0);
    }
}

TEST_F(AssetServiceTest, LoadTimeSeriesValueSharedTimestampsThrowsOnNull) {
    EXPECT_THROW(service->loadTimeSeriesValue(AssetId{AssetType::Cash, "USD"}, TimestampPtr{}), std::invalid_argument);
}
