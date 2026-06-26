// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "finlib/core/TimeSeries.hpp"

using ts::TimeSeries;

// Builds a TimeSeries with values at 1000ms steps starting at t=1000.
inline std::shared_ptr<TimeSeries> makeSeries(std::string id, std::vector<double> vals) {
    std::vector<int64_t> ts;
    ts.reserve(vals.size());
    for (size_t i = 0; i < vals.size(); ++i) ts.push_back(static_cast<int64_t>(i + 1) * 1000);
    return std::make_shared<TimeSeries>(std::move(id), std::move(ts), std::move(vals));
}

// Builds a TimeSeries with explicit timestamps.
inline std::shared_ptr<TimeSeries> makeSeriesAt(std::string id, std::vector<int64_t> ts, std::vector<double> vals) {
    return std::make_shared<TimeSeries>(std::move(id), std::move(ts), std::move(vals));
}

// Base gtest fixture — provides the standard mock TimeSeries used across unit tests.
// Inherit instead of ::testing::Test. Call TimeSeriesMocks::SetUp() if you override SetUp.
class TimeSeriesMocks : public ::testing::Test {
 protected:
    // {1, 2, 3, 4, 5} at t=1000..5000ms
    std::shared_ptr<TimeSeries> simpleSeries;
    // {10, 20, 30, 40, 50} at t=1000..5000ms
    std::shared_ptr<TimeSeries> decadeSeries;
    // {3, 3, 3, 3, 3} at t=1000..5000ms
    std::shared_ptr<TimeSeries> constantSeries;
    // 100-point sin series (amplitude 10) at t=0..99000ms
    std::shared_ptr<TimeSeries> largerSeries;

    void SetUp() override {
        simpleSeries = makeSeries("SimpleSeries", {1.0, 2.0, 3.0, 4.0, 5.0});
        decadeSeries = makeSeries("DecadeSeries", {10.0, 20.0, 30.0, 40.0, 50.0});
        constantSeries = makeSeries("ConstantSeries", {3.0, 3.0, 3.0, 3.0, 3.0});

        std::vector<int64_t> largerTs(100);
        std::vector<double> largerVals(100);
        for (size_t i = 0; i < 100; ++i) {
            largerTs[i] = static_cast<int64_t>(i) * 1000;
            largerVals[i] = std::sin(static_cast<double>(i) * 0.1) * 10.0;
        }
        largerSeries = std::make_shared<TimeSeries>("LargerSeries", std::move(largerTs), std::move(largerVals));
    }
};
