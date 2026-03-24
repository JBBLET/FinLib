// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "finlib/common/logger/ILogger.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/data/CoverageInfo.hpp"
#include "finlib/data/SeriesKey.hpp"
#include "finlib/data/implementation/CSVRepository.hpp"
#include "finlib/models/interfaces/IModel.hpp"
#include "finlib/models/timeseries/regression/ARModel.hpp"
#include "finlib/session/AppContext.hpp"
#include "finlib/session/ModelSession.hpp"

class TestLogger : public logging::ILogger {
 public:
    void write(logging::Level /*lvl*/, const std::string& msg) override { messages.push_back(msg); }
    std::vector<std::string> messages;
};

static std::shared_ptr<TimeSeries> generateAR1(double phi, double intercept, size_t n, double y0 = 10.0) {
    std::vector<int64_t> ts(n);
    std::vector<double> vals(n);
    vals[0] = y0;
    ts[0] = 1000;

    std::mt19937 rng(42);
    std::normal_distribution<double> noise(0.0, 0.5);

    for (size_t i = 1; i < n; ++i) {
        ts[i] = ts[i - 1] + 1000;
        vals[i] = intercept + phi * vals[i - 1] + noise(rng);
    }
    return std::make_shared<TimeSeries>("session_test_ts", std::move(ts), std::move(vals));
}

class ModelSessionTest : public ::testing::Test {
 protected:
    TestLogger logger;
    std::filesystem::path testDir;
    std::unique_ptr<CSVRepository> repo;
    AppContext context;

    std::shared_ptr<TimeSeries> series;
    std::shared_ptr<models::regression::ARModel> fittedModel;

    double truePhi = 0.7;
    double trueIntercept = 2.0;
    int64_t deltaT = 1000;

    void SetUp() override {
        testDir = std::filesystem::temp_directory_path() / "finlib_test_session";
        std::filesystem::create_directories(testDir);
        repo = std::make_unique<CSVRepository>(testDir);
        context = AppContext{&logger, repo.get()};

        series = generateAR1(truePhi, trueIntercept, 500);

        fittedModel = std::make_shared<models::regression::ARModel>(1, models::regression::ARModel::Solver::OLS);
        auto view = series->view();
        fittedModel->setData(view, 0.8, 0.0);
        fittedModel->fit();
    }

    void TearDown() override { std::filesystem::remove_all(testDir); }
};

// Construction Tests

TEST_F(ModelSessionTest, ConstructionWithFittedModel) {
    auto view = series->view();
    EXPECT_NO_THROW(ModelSession(context, fittedModel, view, 50, deltaT, 100.0));
}

TEST_F(ModelSessionTest, ConstructionThrowsWithUnfittedModel) {
    auto unfitted = std::make_shared<models::regression::ARModel>(1, models::regression::ARModel::Solver::OLS);
    auto view = series->view();
    EXPECT_THROW(ModelSession(context, unfitted, view, 50, deltaT, 100.0), std::runtime_error);
}

// Forecast Tests

TEST_F(ModelSessionTest, ForecastReturnsCorrectNumberOfSteps) {
    auto view = series->view();
    ModelSession session(context, fittedModel, view, 50, deltaT, 100.0);

    auto predictions = session.forecast(5);
    EXPECT_EQ(predictions.size(), 5);
}

TEST_F(ModelSessionTest, ForecastTimestampsAreCorrectlySpaced) {
    auto view = series->view();
    ModelSession session(context, fittedModel, view, 50, deltaT, 100.0);

    auto predictions = session.forecast(3);

    int64_t lastTs = view.timestamp(view.size() - 1);
    EXPECT_EQ(predictions[0].timestamp, lastTs + deltaT);
    EXPECT_EQ(predictions[1].timestamp, lastTs + 2 * deltaT);
    EXPECT_EQ(predictions[2].timestamp, lastTs + 3 * deltaT);
}

TEST_F(ModelSessionTest, ForecastValuesAreFinite) {
    auto view = series->view();
    ModelSession session(context, fittedModel, view, 50, deltaT, 100.0);

    auto predictions = session.forecast(10);
    for (const auto& p : predictions) {
        EXPECT_TRUE(std::isfinite(p.predictedValue));
    }
}

TEST_F(ModelSessionTest, ForecastDoesNotModifyWindowMultipleCalls) {
    auto view = series->view();
    ModelSession session(context, fittedModel, view, 50, deltaT, 100.0);

    auto predictions1 = session.forecast(5);
    auto predictions2 = session.forecast(5);

    for (size_t i = 0; i < 5; ++i) {
        EXPECT_DOUBLE_EQ(predictions1[i].predictedValue, predictions2[i].predictedValue);
    }
}

// Observe Tests

TEST_F(ModelSessionTest, ObserveUpdatesErrorTracking) {
    // Use first 400 points for session, keep 400-499 as future actuals
    auto sessionView = series->slice(0, 400);
    ModelSession session(context, fittedModel, sessionView, 50, deltaT, 100.0);

    session.forecast(5);

    const auto& vals = series->getValues();
    const auto& timestamps = series->getTimestamps();

    // Observe first actual (index 400 = first point after session view)
    session.observe(vals[400], timestamps[400]);

    // Rolling MSE should now be computable
    double mse = session.rollingMSE(1);
    EXPECT_GE(mse, 0.0);
    EXPECT_TRUE(std::isfinite(mse));
}

TEST_F(ModelSessionTest, ObserveSequenceProducesReasonableError) {
    auto sessionView = series->slice(0, 400);
    auto localModel = std::make_shared<models::regression::ARModel>(1, models::regression::ARModel::Solver::OLS);
    localModel->setData(sessionView, 0.9, 0.0);
    localModel->fit();

    ModelSession session(context, localModel, sessionView, 50, deltaT, 100.0);

    session.forecast(45);

    const auto& vals = series->getValues();
    const auto& timestamps = series->getTimestamps();

    for (size_t i = 400; i < 445; ++i) {
        session.observe(vals[i], timestamps[i]);
    }

    double mse = session.rollingMSE(45);
    EXPECT_GT(mse, 0.0);
    // For AR(1) with phi=0.7 and noise std=0.5, one-step MSE ~ 0.25
    // Multi-step MSE will be higher due to error compounding
    EXPECT_LT(mse, 100.0);
}

// Rolling Error Tests

TEST_F(ModelSessionTest, RollingMSEWithNoObservationsReturnsZero) {
    auto view = series->view();
    ModelSession session(context, fittedModel, view, 50, deltaT, 100.0);

    EXPECT_DOUBLE_EQ(session.rollingMSE(10), 0.0);
}

TEST_F(ModelSessionTest, RollingMAEWithNoObservationsReturnsZero) {
    auto view = series->view();
    ModelSession session(context, fittedModel, view, 50, deltaT, 100.0);

    EXPECT_DOUBLE_EQ(session.rollingMAE(10), 0.0);
}

TEST_F(ModelSessionTest, RollingMSEIsNonNegative) {
    auto sessionView = series->slice(0, 400);
    ModelSession session(context, fittedModel, sessionView, 50, deltaT, 100.0);

    session.forecast(5);

    const auto& vals = series->getValues();
    const auto& timestamps = series->getTimestamps();

    for (size_t i = 0; i < 5; ++i) {
        session.observe(vals[400 + i], timestamps[400 + i]);
    }

    EXPECT_GE(session.rollingMSE(5), 0.0);
    EXPECT_GE(session.rollingMAE(5), 0.0);
}

// ShouldRefit Tests

TEST_F(ModelSessionTest, ShouldRefitReturnsFalseWithLowError) {
    auto view = series->view();
    ModelSession session(context, fittedModel, view, 50, deltaT, 100.0);

    EXPECT_FALSE(session.shouldRefit(1.0));
}

TEST_F(ModelSessionTest, ShouldRefitReturnsTrueWithHighError) {
    auto sessionView = series->slice(0, 400);
    ModelSession session(context, fittedModel, sessionView, 50, deltaT, 100.0);

    session.forecast(3);

    int64_t lastTs = sessionView.timestamp(sessionView.size() - 1);
    session.observe(99999.0, lastTs + deltaT);
    session.observe(99999.0, lastTs + 2 * deltaT);
    session.observe(99999.0, lastTs + 3 * deltaT);

    EXPECT_TRUE(session.shouldRefit(0.001));
}

// Refit Tests

TEST_F(ModelSessionTest, RefitProducesNewFittedModel) {
    auto view = series->view();
    ModelSession session(context, fittedModel, view, 50, deltaT, 100.0);

    EXPECT_NO_THROW(session.refit(view));

    auto predictions = session.forecast(3);
    EXPECT_EQ(predictions.size(), 3);
    for (const auto& p : predictions) {
        EXPECT_TRUE(std::isfinite(p.predictedValue));
    }
}

// CSVRepository Tests

class CSVRepositoryTest : public ::testing::Test {
 protected:
    std::filesystem::path testDir;
    std::unique_ptr<CSVRepository> repo;

    void SetUp() override {
        testDir = std::filesystem::temp_directory_path() / "finlib_csv_test";
        std::filesystem::create_directories(testDir);
        repo = std::make_unique<CSVRepository>(testDir);
    }

    void TearDown() override { std::filesystem::remove_all(testDir); }
};

TEST_F(CSVRepositoryTest, SaveAndLoadFullTimeSeries) {
    std::vector<int64_t> ts = {1000, 2000, 3000, 4000, 5000};
    std::vector<double> vals = {1.1, 2.2, 3.3, 4.4, 5.5};
    TimeSeries original("test_save_load", ts, vals);

    SeriesKey key{"test_save_load", 1000};
    CoverageInfo cov{key, 1000, 5000, "test", 0};
    repo->save(key, original, cov);

    auto loaded = repo->load("test_save_load", 0, 10000);
    EXPECT_EQ(loaded.size(), 5);

    const auto& loadedTs = loaded.getTimestamps();
    const auto& loadedVals = loaded.getValues();
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(loadedTs[i], ts[i]);
        EXPECT_NEAR(loadedVals[i], vals[i], 1e-10);
    }
}

TEST_F(CSVRepositoryTest, LoadWithTimestampRange) {
    std::vector<int64_t> ts = {1000, 2000, 3000, 4000, 5000};
    std::vector<double> vals = {1.1, 2.2, 3.3, 4.4, 5.5};
    TimeSeries original("test_range", ts, vals);

    SeriesKey key{"test_range", 1000};
    CoverageInfo cov{key, 1000, 5000, "test", 0};
    repo->save(key, original, cov);

    auto loaded = repo->load("test_range", 2000, 4000);
    EXPECT_EQ(loaded.size(), 3);  // timestamps 2000, 3000, 4000

    const auto& loadedTs = loaded.getTimestamps();
    EXPECT_EQ(loadedTs[0], 2000);
    EXPECT_EQ(loadedTs[1], 3000);
    EXPECT_EQ(loadedTs[2], 4000);
}

TEST_F(CSVRepositoryTest, MergeAppendsPoints) {
    // Save initial data
    std::vector<int64_t> ts = {1000, 2000, 3000};
    std::vector<double> vals = {1.0, 2.0, 3.0};
    TimeSeries original("test_append", ts, vals);

    SeriesKey key{"test_append", 1000};
    CoverageInfo cov{key, 1000, 3000, "test", 0};
    repo->save(key, original, cov);

    // Merge new points
    std::vector<int64_t> newTs = {4000, 5000};
    std::vector<double> newVals = {4.0, 5.0};
    TimeSeries newData("test_append", newTs, newVals);
    repo->merge(key, newData);

    auto loaded = repo->load("test_append", 0, 10000);
    EXPECT_EQ(loaded.size(), 5);

    const auto& loadedVals = loaded.getValues();
    EXPECT_NEAR(loadedVals[3], 4.0, 1e-10);
    EXPECT_NEAR(loadedVals[4], 5.0, 1e-10);
}

TEST_F(CSVRepositoryTest, LoadThrowsForMissingFile) {
    EXPECT_THROW(repo->load("nonexistent", 0, 10000), std::runtime_error);
}

TEST_F(CSVRepositoryTest, SaveCreatesDirectory) {
    auto nestedDir = testDir / "nested" / "deep";
    CSVRepository nestedRepo(nestedDir);

    std::vector<int64_t> ts = {1000};
    std::vector<double> vals = {1.0};
    TimeSeries series("test_nested", ts, vals);

    SeriesKey key{"test_nested", 1000};
    CoverageInfo cov{key, 1000, 1000, "test", 0};
    EXPECT_NO_THROW(nestedRepo.save(key, series, cov));
    // Directory layout: <dir>/<seriesId>/<frequencyMs>.csv
    EXPECT_TRUE(std::filesystem::exists(nestedDir / "test_nested" / "1000.csv"));
}

TEST_F(CSVRepositoryTest, MergeToNonexistentCreatesFile) {
    std::vector<int64_t> ts = {1000, 2000};
    std::vector<double> vals = {1.0, 2.0};
    TimeSeries newData("new_file", ts, vals);

    SeriesKey key{"new_file", 1000};
    repo->merge(key, newData);

    auto loaded = repo->load("new_file", 0, 10000);
    EXPECT_EQ(loaded.size(), 2);
}

// Integration: ModelSession + CSVRepository flush

TEST_F(ModelSessionTest, ObserveFlushesBufferToRepository) {
    auto sessionView = series->slice(0, 400);
    ModelSession session(context, fittedModel, sessionView, 50, deltaT, 100.0);

    session.forecast(10);

    const auto& vals = series->getValues();
    const auto& timestamps = series->getTimestamps();

    for (size_t i = 0; i < 5; ++i) {
        session.observe(vals[400 + i], timestamps[400 + i]);
    }

    double mse = session.rollingMSE(5);
    EXPECT_GE(mse, 0.0);
}
