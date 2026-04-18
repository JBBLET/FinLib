// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "finapp/finance/common/Currency.hpp"
#include "finapp/service/FXService.hpp"
#include "finlib/common/utils/TimeSeriesUtils.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/implementation/CachedTimeSeriesRepository.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"
#include "support/service_test_fakes.hpp"

using namespace finance;
using namespace finapp;
using namespace finapp::test;

namespace {

constexpr int64_t kDay = 86'400'000;

class FXServiceTest : public ::testing::Test {
 protected:
    std::shared_ptr<InMemoryTimeSeriesRepository> innerRepo;
    std::shared_ptr<CachedTimeSeriesRepository> cachedRepo;
    std::shared_ptr<FakeTimeSeriesLoader> provider;
    std::shared_ptr<TimeSeriesService> tsService;
    std::shared_ptr<InMemoryFXRepository> fxRepo;
    std::unique_ptr<FXService> service;

    void SetUp() override {
        innerRepo = std::make_shared<InMemoryTimeSeriesRepository>();
        cachedRepo = std::make_shared<CachedTimeSeriesRepository>(innerRepo);
        provider = std::make_shared<FakeTimeSeriesLoader>();
        tsService = std::make_shared<TimeSeriesService>(cachedRepo, provider);
        fxRepo = std::make_shared<InMemoryFXRepository>();
        service = std::make_unique<FXService>(tsService, fxRepo);
    }
};

}  // namespace

TEST_F(FXServiceTest, SameCurrencyReturnsConstantOneSeries) {
    TimeSeries result = service->load(Currency::USD, Currency::USD, 0, 4 * kDay, kDay);
    ASSERT_EQ(result.size(), 5);
    for (double v : result.getValues()) {
        EXPECT_DOUBLE_EQ(v, 1.0);
    }
}

TEST_F(FXServiceTest, SameCurrencySharedTimestampsReturnsConstantOne) {
    auto timestamps = common::utils::timeSeries::makeRegularTimestamps(0, 3 * kDay, kDay);
    TimeSeries result = service->load(Currency::EUR, Currency::EUR, timestamps);
    ASSERT_EQ(result.size(), 4);
    for (double v : result.getValues()) {
        EXPECT_DOUBLE_EQ(v, 1.0);
    }
    // Shared timestamp overload must return a series bound to the caller's vector.
    EXPECT_EQ(result.getSharedTimestamps().get(), timestamps.get());
}

TEST_F(FXServiceTest, LoadExistingPairHitsRepositorySeriesId) {
    // Repo already knows about the pair → service uses the persisted timeseriesID.
    fxRepo->save(FXInfos{Currency::EUR, Currency::USD, "FX:EUR_USD"});
    provider->setSeries("FX:EUR_USD", makeFlatSeries("FX:EUR_USD", 0, 4 * kDay, kDay, 1.08));

    TimeSeries result = service->load(Currency::EUR, Currency::USD, 0, 4 * kDay, kDay);
    ASSERT_EQ(result.size(), 5);
    for (double v : result.getValues()) {
        EXPECT_DOUBLE_EQ(v, 1.08);
    }
}

TEST_F(FXServiceTest, LoadUnknownPairCreatesFXInfosAndFetches) {
    ASSERT_FALSE(fxRepo->exists(Currency::USD, Currency::JPY));
    // Canonical derived id is "<base><quote>=X" (yfinance FX ticker convention).
    provider->setSeries("USDJPY=X", makeFlatSeries("USDJPY=X", 0, 4 * kDay, kDay, 150.0));

    TimeSeries result = service->load(Currency::USD, Currency::JPY, 0, 4 * kDay, kDay);
    ASSERT_EQ(result.size(), 5);
    EXPECT_DOUBLE_EQ(result.getValues().front(), 150.0);

    // The new pair must now be persisted so subsequent calls hit the fast path.
    ASSERT_TRUE(fxRepo->exists(Currency::USD, Currency::JPY));
    EXPECT_EQ(fxRepo->load(Currency::USD, Currency::JPY).timeseriesID, "USDJPY=X");
}

TEST_F(FXServiceTest, SharedTimestampOverloadKeepsPointerAlignment) {
    fxRepo->save(FXInfos{Currency::GBP, Currency::EUR, "GBPEUR"});
    provider->setSeries("GBPEUR", makeFlatSeries("GBPEUR", 0, 6 * kDay, kDay, 1.17));

    auto timestamps = common::utils::timeSeries::makeRegularTimestamps(0, 3 * kDay, kDay);
    TimeSeries a = service->load(Currency::GBP, Currency::EUR, timestamps);
    TimeSeries b = service->load(Currency::GBP, Currency::EUR, timestamps);

    ASSERT_EQ(a.size(), 4);
    ASSERT_EQ(b.size(), 4);
    // Both calls must be bound to the caller's shared vector so downstream operators
    // can short-circuit alignment checks via pointer equality.
    EXPECT_EQ(a.getSharedTimestamps().get(), timestamps.get());
    EXPECT_EQ(b.getSharedTimestamps().get(), timestamps.get());
}
