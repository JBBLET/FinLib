// Copyright (c) 2026 JBBLET. All Rights Reserved.
//
// Verifies the AnalysisFeature layer carries the full generality of
// CustomTimeSeriesAnalysis — not just scalar doubles, but arbitrary return
// types and parameterized metrics — and that type-filtered retrieval works.
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "finapp/finance/analysis/AnalysisFeature.hpp"
#include "finapp/finance/analysis/ReturnFeatures.hpp"
#include "finlib/analysis/MetricHandle.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/session/TimeSeriesSession.hpp"

using finance::analysis::computeMetricsOfType;
using finance::analysis::FeatureBindings;
using finance::analysis::FeatureInstaller;
using finance::analysis::logReturnFeature;
using finance::analysis::retainMetric;
using finance::analysis::RetainedMetric;
using ts::TimeSeries;
using ts::TimeSeriesView;
using ts::analysis::ParameterizedMetricFn;
using ts::analysis::TimeSeriesSession;

namespace {

std::shared_ptr<TimeSeriesSession> makeSession() {
    std::vector<int64_t> timestamps;
    std::vector<double> values;
    for (int i = 0; i < 8; ++i) {
        timestamps.push_back(static_cast<int64_t>(i) * 86'400'000);  // daily
        values.push_back(100.0 + i);                                 // 100..107, strictly increasing
    }
    auto series = std::make_shared<const TimeSeries>("TEST", std::move(timestamps), std::move(values));
    return std::make_shared<TimeSeriesSession>(series);
}

// A feature that registers a NON-scalar metric (returns the series values) and
// a PARAMETERIZED metric (returns the value at a baked-in index).
FeatureInstaller customMetricsFeature() {
    return [](ts::analysis::ITimeSeriesSession& session, const std::string& base) {
        FeatureBindings bindings;

        auto vectorHandle = session.customAnalysis(base).addMetric<std::vector<double>>(
            base, "identity", [](TimeSeriesView v) {
                std::vector<double> out;
                out.reserve(v.size());
                for (size_t i = 0; i < v.size(); ++i) out.push_back(v[i]);
                return out;
            });
        bindings.metrics.push_back(retainMetric(base, "identity", vectorHandle));

        auto paramHandle = session.customAnalysis(base).addMetric<double, int>(
            base, "elementAt", ParameterizedMetricFn<double, int>([](TimeSeriesView v, const int& i) {
                return v[static_cast<size_t>(i)];
            }));
        bindings.metrics.push_back(retainMetric(base, "elementAt", paramHandle, /*param=*/3));

        return bindings;
    };
}

}  // namespace

TEST(AnalysisFeatureTest, RetainsNonScalarAndParameterizedMetrics) {
    auto session = makeSession();
    const std::string base = "";

    std::vector<RetainedMetric> metrics;
    auto append = [&](const FeatureInstaller& f) {
        for (auto& m : f(*session, base).metrics) metrics.push_back(std::move(m));
    };
    append(customMetricsFeature());

    // A vector<double> metric is reachable when asking for that type...
    auto vecMetrics = computeMetricsOfType<std::vector<double>>(*session, metrics);
    ASSERT_EQ(vecMetrics.size(), 1u);
    EXPECT_EQ(vecMetrics[0].first, "identity");
    ASSERT_EQ(vecMetrics[0].second.size(), 8u);
    EXPECT_DOUBLE_EQ(vecMetrics[0].second.front(), 100.0);
    EXPECT_DOUBLE_EQ(vecMetrics[0].second.back(), 107.0);

    // ...and the parameterized double metric computes with its baked-in index (3 -> 103).
    auto scalarMetrics = computeMetricsOfType<double>(*session, metrics);
    ASSERT_EQ(scalarMetrics.size(), 1u);
    EXPECT_EQ(scalarMetrics[0].first, "elementAt");
    EXPECT_DOUBLE_EQ(scalarMetrics[0].second, 103.0);
}

TEST(AnalysisFeatureTest, TypeFilteringSeparatesScalarsFromOtherResults) {
    auto session = makeSession();
    const std::string base = "";

    std::vector<RetainedMetric> metrics;
    // logReturnFeature adds two scalar (double) metrics on the "logReturn" series.
    for (auto& m : logReturnFeature()(*session, base).metrics) metrics.push_back(std::move(m));
    for (auto& m : customMetricsFeature()(*session, base).metrics) metrics.push_back(std::move(m));

    auto scalars = computeMetricsOfType<double>(*session, metrics);
    auto vectors = computeMetricsOfType<std::vector<double>>(*session, metrics);

    // The vector metric never leaks into the scalar view.
    for (const auto& [name, _] : scalars) EXPECT_NE(name, "identity");
    EXPECT_EQ(vectors.size(), 1u);

    // Sharpe + annualizedVolatility (from logReturn) and elementAt (parameterized) are all doubles.
    bool hasSharpe = false, hasVol = false, hasElementAt = false;
    for (const auto& [name, _] : scalars) {
        hasSharpe |= (name == "sharpe");
        hasVol |= (name == "annualizedVolatility");
        hasElementAt |= (name == "elementAt");
    }
    EXPECT_TRUE(hasSharpe);
    EXPECT_TRUE(hasVol);
    EXPECT_TRUE(hasElementAt);
}
