// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include "TestMockTimeSeries.hpp"
#include "finlib/analysis/CustomTimeSeriesAnalysis.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/session/MultiTimeSeriesSession.hpp"
#include "finlib/session/TimeSeriesSession.hpp"

static double viewSum(TimeSeriesView v) {
    double s = 0.0;
    for (size_t i = 0; i < v.size(); ++i) s += v[i];
    return s;
}

// ============================================================
// TimeSeriesSession
// ============================================================

class TimeSeriesSessionTest : public TimeSeriesMocks {
 protected:
    std::shared_ptr<TimeSeries> ts_;
    std::unique_ptr<analysis::TimeSeriesSession> session_;

    void SetUp() override {
        TimeSeriesMocks::SetUp();
        ts_ = simpleSeries;
        session_ = std::make_unique<analysis::TimeSeriesSession>(ts_);
    }
};

TEST_F(TimeSeriesSessionTest, SourcePtrIsNotNull) { EXPECT_NE(session_->sourceTimeSeriesPtr(), nullptr); }

TEST_F(TimeSeriesSessionTest, SeriesPtrEmptyNameReturnsSource) {
    EXPECT_EQ(session_->seriesPtr(""), session_->sourceTimeSeriesPtr());
}

TEST_F(TimeSeriesSessionTest, SourceViewHasCorrectSize) { EXPECT_EQ(session_->sourceView().size(), 5u); }

TEST_F(TimeSeriesSessionTest, SourceViewHasCorrectValues) {
    auto v = session_->sourceView();
    for (size_t i = 0; i < v.size(); ++i) EXPECT_DOUBLE_EQ(v[i], static_cast<double>(i + 1));
}

TEST_F(TimeSeriesSessionTest, ScalarMultiplyTransform) {
    session_->addTransform("scaled", [](const TimeSeries& src) { return src * 2.0; });
    auto derived = session_->derivedTimeSeriesPtr("scaled");
    ASSERT_NE(derived, nullptr);
    const auto& vals = derived->getValues();
    ASSERT_EQ(vals.size(), 5u);
    for (size_t i = 0; i < vals.size(); ++i) EXPECT_DOUBLE_EQ(vals[i], static_cast<double>(i + 1) * 2.0);
}

TEST_F(TimeSeriesSessionTest, DerivedViewViaSeriesView) {
    session_->addTransform("scaled", [](const TimeSeries& src) { return src * 3.0; });
    auto v = session_->seriesView("scaled");
    EXPECT_EQ(v.size(), 5u);
    EXPECT_DOUBLE_EQ(v[0], 3.0);
    EXPECT_DOUBLE_EQ(v[4], 15.0);
}

TEST_F(TimeSeriesSessionTest, DerivedAnalysisMeanIsCorrect) {
    // source mean = 3.0, scaled by 2 → derived mean = 6.0
    session_->addTransform("scaled", [](const TimeSeries& src) { return src * 2.0; });
    const auto& analysis = session_->seriesAnalysis("scaled");
    EXPECT_DOUBLE_EQ(analysis.mean(), 6.0);
}

TEST_F(TimeSeriesSessionTest, DependencyChainDerivedFromDerived) {
    // "doubled" = source * 2, "quadrupled" = doubled * 2 (sum-of-one pattern)
    session_->addTransform("doubled", [](const TimeSeries& src) { return src * 2.0; });
    session_->addTransform(
        "quadrupled", {"doubled"}, [](std::unordered_map<std::string, std::shared_ptr<const TimeSeries>> m) {
            return *m.at("doubled") * 2.0;
        });
    auto v = session_->derivedView("quadrupled");
    EXPECT_DOUBLE_EQ(v[0], 4.0);
    EXPECT_DOUBLE_EQ(v[4], 20.0);
}

TEST_F(TimeSeriesSessionTest, SumTransformFromSourceAndDerived) {
    // "doubled" = source * 2; "sum" = source + doubled
    session_->addTransform("doubled", [](const TimeSeries& src) { return src * 2.0; });
    session_->addTransform(
        "sum", {"source", "doubled"}, [](std::unordered_map<std::string, std::shared_ptr<const TimeSeries>> m) {
            return *m.at("source") + *m.at("doubled");
        });
    auto v = session_->derivedView("sum");
    // source[i]*(1+2) = i*3
    EXPECT_DOUBLE_EQ(v[0], 3.0);
    EXPECT_DOUBLE_EQ(v[4], 15.0);
}

TEST_F(TimeSeriesSessionTest, UnknownTransformThrows) {
    EXPECT_THROW(session_->seriesPtr("nonexistent"), std::logic_error);
}

TEST_F(TimeSeriesSessionTest, DerivedCacheReturnsSamePointer) {
    session_->addTransform("scaled", [](const TimeSeries& src) { return src * 2.0; });
    auto p1 = session_->derivedTimeSeriesPtr("scaled");
    auto p2 = session_->derivedTimeSeriesPtr("scaled");
    EXPECT_EQ(p1, p2);
}

TEST_F(TimeSeriesSessionTest, CustomAnalysisOnSourceComputesSum) {
    auto& ca = session_->customAnalysis();
    auto handle = ca.addMetric<double>("", "sum", [](TimeSeriesView v) { return viewSum(v); });
    EXPECT_DOUBLE_EQ(ca.compute(handle), 15.0);
}

TEST_F(TimeSeriesSessionTest, CustomAnalysisOnDerivedComputesSum) {
    session_->addTransform("scaled", [](const TimeSeries& src) { return src * 2.0; });
    auto& ca = session_->customAnalysis("scaled");
    auto handle = ca.addMetric<double>("scaled", "sum", [](TimeSeriesView v) { return viewSum(v); });
    EXPECT_DOUBLE_EQ(ca.compute(handle), 30.0);
}

// ============================================================
// MultiTimeSeriesSession
// ============================================================

class MultiTimeSeriesSessionTest : public TimeSeriesMocks {
 protected:
    std::shared_ptr<TimeSeries> tsA_, tsB_;
    std::shared_ptr<analysis::TimeSeriesSession> sessionA_, sessionB_;
    std::unique_ptr<analysis::MultiTimeSeriesSession> multi_;

    void SetUp() override {
        TimeSeriesMocks::SetUp();
        tsA_ = simpleSeries;
        tsB_ = decadeSeries;
        sessionA_ = std::make_shared<analysis::TimeSeriesSession>(tsA_);
        sessionB_ = std::make_shared<analysis::TimeSeriesSession>(tsB_);
        multi_ = std::make_unique<analysis::MultiTimeSeriesSession>();
        multi_->addSession("A", sessionA_);
        multi_->addSession("B", sessionB_);
    }
};

TEST_F(MultiTimeSeriesSessionTest, SessionNamesContainRegistered) {
    auto names = multi_->sessionNames();
    EXPECT_EQ(names.size(), 2u);
    bool hasA = false, hasB = false;
    for (const auto& n : names) {
        if (n == "A") hasA = true;
        if (n == "B") hasB = true;
    }
    EXPECT_TRUE(hasA);
    EXPECT_TRUE(hasB);
}

TEST_F(MultiTimeSeriesSessionTest, SeriesPtrEmptyNameThrows) { EXPECT_THROW(multi_->seriesPtr(""), std::logic_error); }

TEST_F(MultiTimeSeriesSessionTest, SumCrossTransform) {
    multi_->addTransform(
        "sum", {"A", "B"}, [](const std::unordered_map<std::string, std::shared_ptr<const TimeSeries>>& m) {
            return *m.at("A") + *m.at("B");
        });
    auto result = multi_->seriesPtr("sum");
    ASSERT_NE(result, nullptr);
    const auto& vals = result->getValues();
    ASSERT_EQ(vals.size(), 5u);
    EXPECT_DOUBLE_EQ(vals[0], 11.0);
    EXPECT_DOUBLE_EQ(vals[1], 22.0);
    EXPECT_DOUBLE_EQ(vals[4], 55.0);
}

TEST_F(MultiTimeSeriesSessionTest, SubSeriesViewDelegatesCorrectly) {
    auto v = multi_->subSeriesView("A");
    EXPECT_EQ(v.size(), 5u);
    EXPECT_DOUBLE_EQ(v[0], 1.0);
    EXPECT_DOUBLE_EQ(v[4], 5.0);
}

TEST_F(MultiTimeSeriesSessionTest, SubSeriesAnalysisMeanIsCorrect) {
    const auto& analysis = multi_->subSeriesAnalysis("B");
    EXPECT_DOUBLE_EQ(analysis.mean(), 30.0);
}

TEST_F(MultiTimeSeriesSessionTest, CrossTransformCacheReturnsSamePointer) {
    multi_->addTransform(
        "sum", {"A", "B"}, [](const std::unordered_map<std::string, std::shared_ptr<const TimeSeries>>& m) {
            return *m.at("A") + *m.at("B");
        });
    auto p1 = multi_->seriesPtr("sum");
    auto p2 = multi_->seriesPtr("sum");
    EXPECT_EQ(p1, p2);
}

TEST_F(MultiTimeSeriesSessionTest, ScalarMultiplyCrossTransform) {
    multi_->addTransform(
        "scaled", {"A"}, [](const std::unordered_map<std::string, std::shared_ptr<const TimeSeries>>& m) {
            return *m.at("A") * 5.0;
        });
    auto v = multi_->seriesView("scaled");
    EXPECT_DOUBLE_EQ(v[0], 5.0);
    EXPECT_DOUBLE_EQ(v[4], 25.0);
}

TEST_F(MultiTimeSeriesSessionTest, CustomAnalysisOnCrossTransform) {
    multi_->addTransform(
        "sum", {"A", "B"}, [](const std::unordered_map<std::string, std::shared_ptr<const TimeSeries>>& m) {
            return *m.at("A") + *m.at("B");
        });
    auto& ca = multi_->customAnalysis("sum");
    auto handle = ca.addMetric<double>("sum", "total", [](TimeSeriesView v) { return viewSum(v); });
    // 11+22+33+44+55 = 165
    EXPECT_DOUBLE_EQ(ca.compute(handle), 165.0);
}

TEST_F(MultiTimeSeriesSessionTest, SubCustomAnalysisDelegates) {
    auto& ca = multi_->subCustomAnalysis("A");
    auto handle = ca.addMetric<double>("", "sum", [](TimeSeriesView v) { return viewSum(v); });
    EXPECT_DOUBLE_EQ(ca.compute(handle), 15.0);
}

// ============================================================
// CustomTimeSeriesAnalysis
// ============================================================

class CustomTimeSeriesAnalysisTest : public TimeSeriesMocks {
 protected:
    std::shared_ptr<TimeSeries> tsA_, tsB_;

    void SetUp() override {
        TimeSeriesMocks::SetUp();
        tsA_ = simpleSeries;
        tsB_ = decadeSeries;
    }
};

TEST_F(CustomTimeSeriesAnalysisTest, SingleSeriesSumMetric) {
    analysis::CustomTimeSeriesAnalysis ca("A", tsA_->view());
    auto h = ca.addMetric<double>("A", "sum", [](TimeSeriesView v) { return viewSum(v); });
    EXPECT_DOUBLE_EQ(ca.compute(h), 15.0);
}

TEST_F(CustomTimeSeriesAnalysisTest, ScalarMultiplyMetric) {
    analysis::CustomTimeSeriesAnalysis ca("A", tsA_->view());
    auto h = ca.addMetric<double>("A", "scaledSum", [](TimeSeriesView v) {
        double s = 0.0;
        for (size_t i = 0; i < v.size(); ++i) s += v[i] * 3.0;
        return s;
    });
    EXPECT_DOUBLE_EQ(ca.compute(h), 45.0);
}

TEST_F(CustomTimeSeriesAnalysisTest, MetricResultIsCached) {
    analysis::CustomTimeSeriesAnalysis ca("A", tsA_->view());
    int callCount = 0;
    auto h = ca.addMetric<double>("A", "sum", [&callCount](TimeSeriesView v) {
        ++callCount;
        return viewSum(v);
    });
    ca.compute(h);
    ca.compute(h);
    EXPECT_EQ(callCount, 1);
}

TEST_F(CustomTimeSeriesAnalysisTest, InvalidateCacheForcesRecompute) {
    analysis::CustomTimeSeriesAnalysis ca("A", tsA_->view());
    int callCount = 0;
    auto h = ca.addMetric<double>("A", "sum", [&callCount](TimeSeriesView v) {
        ++callCount;
        return viewSum(v);
    });
    ca.compute(h);
    ca.invalidateCache();
    ca.compute(h);
    EXPECT_EQ(callCount, 2);
}

TEST_F(CustomTimeSeriesAnalysisTest, MultiSeriesSumMetric) {
    std::unordered_map<std::string, TimeSeriesView> views{{"A", tsA_->view()}, {"B", tsB_->view()}};
    analysis::CustomTimeSeriesAnalysis ca(std::move(views));
    auto h =
        ca.addMetric<double>({"A", "B"}, "crossSum", [](const std::unordered_map<std::string, TimeSeriesView>& vs) {
            return viewSum(vs.at("A")) + viewSum(vs.at("B"));
        });
    // 15 + 150 = 165
    EXPECT_DOUBLE_EQ(ca.compute(h), 165.0);
}

TEST_F(CustomTimeSeriesAnalysisTest, RebindUpdatesResult) {
    analysis::CustomTimeSeriesAnalysis ca("A", tsA_->view());
    auto h = ca.addMetric<double>("A", "sum", [](TimeSeriesView v) { return viewSum(v); });
    EXPECT_DOUBLE_EQ(ca.compute(h), 15.0);

    auto tsC = makeSeries("A", {100.0, 200.0});
    ca.rebind(tsC->view());
    EXPECT_DOUBLE_EQ(ca.compute(h), 300.0);
}

TEST_F(CustomTimeSeriesAnalysisTest, AddMetricForUnknownSeriesThrows) {
    analysis::CustomTimeSeriesAnalysis ca("A", tsA_->view());
    EXPECT_THROW(ca.addMetric<double>("unknown", "sum", [](TimeSeriesView v) { return viewSum(v); }),
                 std::invalid_argument);
}

TEST_F(CustomTimeSeriesAnalysisTest, ParameterizedMetricScalesSum) {
    analysis::CustomTimeSeriesAnalysis ca("A", tsA_->view());
    auto h = ca.addMetric<double, double>(
        "A", "scaledSum", analysis::ParameterizedMetricFn<double, double>{[](TimeSeriesView v, const double& factor) {
            return viewSum(v) * factor;
        }});
    EXPECT_DOUBLE_EQ(ca.compute(h, 2.0), 30.0);
    EXPECT_DOUBLE_EQ(ca.compute(h, 0.5), 7.5);
}
