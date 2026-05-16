// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/models/interfaces/EvaluationResult.hpp"
#include "finlib/models/timeseries/regression/ARModel.hpp"

// Helper: generate a synthetic AR(1) process
// y_t = intercept + phi * y_{t-1} + noise[t]
// Using deterministic "noise" so tests are reproducible

static std::shared_ptr<TimeSeries> generateAR1(double phi, double intercept, size_t n, double y0 = 10.0) {
    std::vector<int64_t> ts(n);
    std::vector<double> vals(n);
    vals[0] = y0;
    ts[0] = 1000;

    std::mt19937 rng(42);  // fixed seed = deterministic tests
    std::normal_distribution<double> noise(0.0, 0.5);

    for (size_t i = 1; i < n; ++i) {
        ts[i] = ts[i - 1] + 1000;
        vals[i] = intercept + phi * vals[i - 1] + noise(rng);
    }
    return std::make_shared<TimeSeries>("AR1Model_Test_TimeSeries", std::move(ts), std::move(vals));
}

// Helper: generate a synthetic AR(2) process
// y_t = intercept + phi1 * y_{t-1} + phi2 * y_{t-2} + noise[t]
static std::shared_ptr<TimeSeries> generateAR2(double phi1, double phi2, double intercept, size_t n, double y0 = 10.0,
                                               double y1 = 11.0) {
    std::vector<int64_t> ts(n);
    std::vector<double> vals(n);
    vals[0] = y0;
    vals[1] = y1;
    ts[0] = 1000;
    ts[1] = 2000;
    for (size_t i = 2; i < n; ++i) {
        ts[i] = ts[i - 1] + 1000;
        double noise = 0.01 * std::sin(static_cast<double>(i));
        vals[i] = intercept + phi1 * vals[i - 1] + phi2 * vals[i - 2] + noise;
    }
    return std::make_shared<TimeSeries>("AR2Model_Test_TimeSeries", std::move(ts), std::move(vals));
}

// Helper: generate irregularly spaced data
static std::shared_ptr<TimeSeries> generateIrregular(size_t n) {
    std::vector<int64_t> ts(n);
    std::vector<double> vals(n);
    ts[0] = 1000;
    vals[0] = 1.0;
    for (size_t i = 1; i < n; ++i) {
        ts[i] = ts[i - 1] + 1000 + (i % 3) * 500;  // irregular gaps
        vals[i] = vals[i - 1] + 0.5;
    }
    return std::make_shared<TimeSeries>("IrregularARModel_Test_TimeSeries", std::move(ts), std::move(vals));
}

// ============================================================
// ARModel Construction Tests
// ============================================================

class ARModelTest : public ::testing::Test {
 protected:
    std::shared_ptr<TimeSeries> ar1Series;
    std::shared_ptr<TimeSeries> ar2Series;

    double truePhi1 = 0.7;
    double trueIntercept1 = 2.0;

    double truePhi2_1 = 0.5;
    double truePhi2_2 = 0.3;
    double trueIntercept2 = 1.0;

    void SetUp() override {
        ar1Series = generateAR1(truePhi1, trueIntercept1, 500);
        ar2Series = generateAR2(truePhi2_1, truePhi2_2, trueIntercept2, 500);
    }
};

// ============================================================
// Solver Tests — AR(1) coefficient recovery
// ============================================================

TEST_F(ARModelTest, OLSSolverRecoversAR1Coefficients) {
    models::regression::ARModel model(1, models::regression::ARModel::Solver::OLS);
    auto view = ar1Series->view();
    model.setData(view, 0.8, 0.0);
    model.fit();

    EXPECT_TRUE(model.isStationary());
}

TEST_F(ARModelTest, YuleWalkerSolverRecoversAR1Coefficients) {
    models::regression::ARModel model(1, models::regression::ARModel::Solver::YuleWalker);
    auto view = ar1Series->view();
    model.setData(view, 0.8, 0.0);
    model.fit();

    EXPECT_TRUE(model.isStationary());
}

TEST_F(ARModelTest, LevinsonDurbinSolverRecoversAR1Coefficients) {
    models::regression::ARModel model(1, models::regression::ARModel::Solver::LevinsonDurbin);
    auto view = ar1Series->view();
    model.setData(view, 0.8, 0.0);
    model.fit();

    EXPECT_TRUE(model.isStationary());
}

// ============================================================
// Solver Tests — AR(2) coefficient recovery
// ============================================================

TEST_F(ARModelTest, OLSSolverRecoversAR2Coefficients) {
    models::regression::ARModel model(2, models::regression::ARModel::Solver::OLS);
    auto view = ar2Series->view();
    model.setData(view, 0.8, 0.0);
    model.fit();

    EXPECT_TRUE(model.isStationary());
}

TEST_F(ARModelTest, YuleWalkerSolverRecoversAR2Coefficients) {
    models::regression::ARModel model(2, models::regression::ARModel::Solver::YuleWalker);
    auto view = ar2Series->view();
    model.setData(view, 0.8, 0.0);
    model.fit();

    EXPECT_TRUE(model.isStationary());
}

TEST_F(ARModelTest, LevinsonDurbinSolverRecoversAR2Coefficients) {
    models::regression::ARModel model(2, models::regression::ARModel::Solver::LevinsonDurbin);
    auto view = ar2Series->view();
    model.setData(view, 0.8, 0.0);
    model.fit();

    EXPECT_TRUE(model.isStationary());
}

// ============================================================
// predictOneStep Tests
// ============================================================

TEST_F(ARModelTest, PredictOneStepThrowsWhenNotFitted) {
    models::regression::ARModel model(1);
    Eigen::VectorXd window(1);
    window << 5.0;

    EXPECT_THROW(model.predictOneStep(window), std::runtime_error);
}

TEST_F(ARModelTest, PredictOneStepReturnsFiniteValue) {
    models::regression::ARModel model(1, models::regression::ARModel::Solver::OLS);
    auto view = ar1Series->view();
    model.setData(view, 0.8, 0.0);
    model.fit();

    // Use the last value of training data as the window
    Eigen::VectorXd window(1);
    window << ar1Series->getValues()[399];  // last point of 80% of 500

    double prediction = model.predictOneStep(window);
    EXPECT_TRUE(std::isfinite(prediction));
}

TEST_F(ARModelTest, PredictOneStepAR2) {
    models::regression::ARModel model(2, models::regression::ARModel::Solver::OLS);
    auto view = ar2Series->view();
    model.setData(view, 0.8, 0.0);
    model.fit();

    Eigen::VectorXd window(2);
    window << ar2Series->getValues()[398], ar2Series->getValues()[399];

    double prediction = model.predictOneStep(window);
    EXPECT_TRUE(std::isfinite(prediction));

    // The prediction should be reasonably close to the actual next value
    double actual = ar2Series->getValues()[400];
    EXPECT_NEAR(prediction, actual, std::abs(actual) * 0.1);  // within 10%
}

// ============================================================
// evaluate Tests
// ============================================================

TEST_F(ARModelTest, EvaluateThrowsWhenNotFitted) {
    models::regression::ARModel model(1);
    auto view = ar1Series->view();
    model.setData(view, 0.8, 0.1);

    auto testView = ar1Series->slice(450, 50);
    EXPECT_THROW(model.evaluate(testView), std::runtime_error);
}

TEST_F(ARModelTest, EvaluateProducesValidMetrics) {
    models::regression::ARModel model(1, models::regression::ARModel::Solver::OLS);
    auto view = ar1Series->view();
    model.setData(view, 0.8, 0.0);
    model.fit();

    auto testView = ar1Series->slice(400, 100);
    auto result = model.evaluate(testView);

    // All regression metrics should be populated
    EXPECT_TRUE(result.mse.has_value());
    EXPECT_TRUE(result.rmse.has_value());
    EXPECT_TRUE(result.mae.has_value());
    EXPECT_TRUE(result.rSquared.has_value());
    EXPECT_TRUE(result.logLikelihood.has_value());
    EXPECT_TRUE(result.aic.has_value());

    // Sanity checks on metric values
    EXPECT_GE(result.mse.value(), 0.0);
    EXPECT_GE(result.rmse.value(), 0.0);
    EXPECT_GE(result.mae.value(), 0.0);
    EXPECT_DOUBLE_EQ(result.rmse.value(), std::sqrt(result.mse.value()));
    EXPECT_LE(result.rSquared.value(), 1.0);

    // For near-perfect synthetic data, R^2 should be very high
    EXPECT_NEAR(result.rSquared.value(), truePhi1 * truePhi1, 0.1);
}

// ============================================================
// Regularity enforcement Tests
// ============================================================

TEST_F(ARModelTest, SetDataThrowsOnIrregularData) {
    auto irregular = generateIrregular(200);
    models::regression::ARModel model(1);
    auto view = irregular->view();

    EXPECT_THROW(model.setData(view, 0.8, 0.0), std::runtime_error);
}

TEST_F(ARModelTest, SetDataAcceptsRegularData) {
    models::regression::ARModel model(1);
    auto view = ar1Series->view();

    EXPECT_NO_THROW(model.setData(view, 0.8, 0.0));
}

// ============================================================
// Edge Cases
// ============================================================

TEST_F(ARModelTest, FitThrowsWithInsufficientData) {
    auto small = std::make_shared<TimeSeries>("ThrowInsufficientDataTS", std::vector<int64_t>{1000, 2000, 3000},
                                              std::vector<double>{1.0, 2.0, 3.0});
    models::regression::ARModel model(5);  // AR(5) on 3 points
    auto view = small->view();
    model.setData(view, 1.0, 0.0);

    EXPECT_THROW(model.fit(), std::runtime_error);
}

TEST_F(ARModelTest, StationaryCheckReturnsFalseWhenNotFitted) {
    models::regression::ARModel model(1);
    EXPECT_FALSE(model.isStationary());
}

TEST_F(ARModelTest, NameReturnsCorrectFormat) {
    models::regression::ARModel model1(1);
    EXPECT_EQ(model1.name(), "AR (1)");

    models::regression::ARModel model5(5);
    EXPECT_EQ(model5.name(), "AR (5)");
}

// ============================================================
// EvaluationResult::computeRegressionMetrics Tests
// ============================================================

TEST(EvaluationResultTest, PerfectPredictionGivesZeroError) {
    std::vector<double> actual = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> prediction = {1.0, 2.0, 3.0, 4.0, 5.0};

    models::RegressionEvaluation result;
    result.computeRegressionMetrics(actual, prediction, 1, 1.0);

    EXPECT_TRUE(result.mse.has_value());
    EXPECT_DOUBLE_EQ(result.mse.value(), 0.0);
    EXPECT_DOUBLE_EQ(result.rmse.value(), 0.0);
    EXPECT_DOUBLE_EQ(result.mae.value(), 0.0);
    EXPECT_DOUBLE_EQ(result.rSquared.value(), 1.0);
}

TEST(EvaluationResultTest, KnownErrorValues) {
    std::vector<double> actual = {1.0, 2.0, 3.0, 4.0};
    std::vector<double> prediction = {1.5, 2.5, 3.5, 4.5};
    // Each residual = -0.5, squared = 0.25
    // MSE = 0.25, RMSE = 0.5, MAE = 0.5

    models::RegressionEvaluation result;
    result.computeRegressionMetrics(actual, prediction, 1, 1.0);

    EXPECT_TRUE(result.mse.has_value());
    EXPECT_NEAR(result.mse.value(), 0.25, 1e-10);
    EXPECT_NEAR(result.rmse.value(), 0.5, 1e-10);
    EXPECT_NEAR(result.mae.value(), 0.5, 1e-10);
}

TEST(EvaluationResultTest, ThrowsOnSizeMismatch) {
    std::vector<double> actual = {1.0, 2.0, 3.0};
    std::vector<double> prediction = {1.0, 2.0};

    models::RegressionEvaluation result;
    EXPECT_THROW(result.computeRegressionMetrics(actual, prediction, 1, 1.0), std::runtime_error);
}

TEST(EvaluationResultTest, ThrowsOnEmptyInput) {
    std::vector<double> actual = {};
    std::vector<double> prediction = {};

    models::RegressionEvaluation result;
    EXPECT_THROW(result.computeRegressionMetrics(actual, prediction, 1, 1.0), std::runtime_error);
}

// ============================================================
// Regularity Check Tests
// ============================================================

TEST(RegularityCheckTest, RegularDataIsDetected) {
    std::vector<int64_t> ts = {1000, 2000, 3000, 4000, 5000};
    std::vector<double> vals = {1.0, 2.0, 3.0, 4.0, 5.0};
    auto series = std::make_shared<TimeSeries>("TestRegularDataTS", ts, vals);
    auto view = series->view();

    auto check = view.checkRegularity(0.2);
    EXPECT_TRUE(check.isRegular);
    EXPECT_EQ(check.medianDeltaT, 1000);
    EXPECT_DOUBLE_EQ(check.standardDeviationDeltaT, 0.0);
}

TEST(RegularityCheckTest, IrregularDataIsDetected) {
    auto irregular = generateIrregular(100);
    auto view = irregular->view();

    auto check = view.checkRegularity(0.05);  // tight tolerance
    EXPECT_FALSE(check.isRegular);
}

TEST(RegularityCheckTest, TinySeriesIsAlwaysRegular) {
    std::vector<int64_t> ts = {1000, 2000};
    std::vector<double> vals = {1.0, 2.0};
    auto series = std::make_shared<TimeSeries>("TestRegularityTS", ts, vals);
    auto view = series->view();

    auto check = view.checkRegularity(0.0);
    EXPECT_TRUE(check.isRegular);
}
