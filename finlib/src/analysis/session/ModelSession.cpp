// Copyright 2026 JBBLET

#include "finlib/analysis/session/ModelSession.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <format>
#include <memory>
#include <print>
#include <string>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "finlib/analysis/models/interfaces/IRegressionModel.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/Format.hpp"
#include "finlib/common/Log.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/data/SeriesKey.hpp"

namespace ts {

std::vector<ModelSession::PredictionEntry> ModelSession::forecast(size_t steps) {
    Timestamp nextPredictedTimeStamp = lastActualTimeStamp_ + deltaT_;
    Eigen::VectorXd tempWindow = window_;
    std::vector<ModelSession::PredictionEntry> output;
    output.reserve(steps);
    for (size_t i = 0; i < steps; ++i) {
        double predictedValue = model_->predictOneStep(tempWindow);
        PredictionEntry entry{nextPredictedTimeStamp, predictedValue};
        output.push_back(entry);
        predictionContainer_.push_back(entry);
        nextPredictedTimeStamp += deltaT_;

        // TODO(JBBLET) Change this to not use a copy of the window_ go from O(windowSize_) to O(1) using head
        // pointers;
        tempWindow.head(windowSize_ - 1) = tempWindow.tail(windowSize_ - 1).eval();
        tempWindow(windowSize_ - 1) = predictedValue;
    }
    return output;
}

void ModelSession::observe(double value, Timestamp timestamp) {
    if (nextToFill_ >= predictionContainer_.size()) {
        return;  // or throw if this should never happen
    }

    PredictionEntry& entry = predictionContainer_[nextToFill_];

    if (std::abs(entry.timestamp - timestamp) > deltaTTolerance_) {
        logging::warn("Timestamp generated does not match any timestamp at which the actual value was received");
    }
    entry.actualValue = value;
    writeBuffer_.push_back(std::pair<Timestamp, double>(timestamp, value));
    if (writeBuffer_.size() > writeBufferCapacity_) flush_();
    double error = value - entry.predictedValue;

    runningSumSquaredError_ += error * error;
    runningSumAbsoluteError_ += std::abs(error);
    ++observationCount_;

    // TODO(JBBLET) Change this to not use a copy of the window_ go from O(windowSize_) to O(1) using head pointers;
    window_.head(windowSize_ - 1) = window_.tail(windowSize_ - 1).eval();
    window_(windowSize_ - 1) = value;

    if (predictionContainer_.size() > errorTrackingWindowSize_) {
        predictionContainer_.pop_front();
        --nextToFill_;
    } else {
        ++nextToFill_;
    }
    lastActualTimeStamp_ = timestamp;
}

double ModelSession::rollingMSE(size_t lastN) const {
    size_t count = 0;
    double sum = 0.0;

    for (auto it = predictionContainer_.rbegin(); it != predictionContainer_.rend() && count < lastN; ++it) {
        if (it->actualValue.has_value()) {
            double err = it->actualValue.value() - it->predictedValue;
            sum += err * err;
            ++count;
        }
    }

    return count > 0 ? sum / count : 0.0;
}

double ModelSession::rollingMAE(size_t lastN) const {
    size_t count = 0;
    double sum = 0.0;

    for (auto it = predictionContainer_.rbegin(); it != predictionContainer_.rend() && count < lastN; ++it) {
        if (it->actualValue.has_value()) {
            double err = it->actualValue.value() - it->predictedValue;
            sum += std::abs(err);
            ++count;
        }
    }

    return count > 0 ? sum / count : 0.0;
}

bool ModelSession::shouldRefit(double mseTreshold) const {
    return (rollingMSE(errorTrackingWindowSize_) > mseTreshold);
}

void ModelSession::refit(const TimeSeriesView& newData) {
    flush_();
    model_ = model_->refitted(newData);
    window_ = newData.asEigenVector().tail(windowSize_);
    lastActualTimeStamp_ = newData.timestamp(newData.size() - 1);
}

std::string ModelSession::toString(const fmt::FormatSpec& spec) const {
    // Entries ahead of nextToFill_ are forecasts still waiting for their actual; behind it,
    // matched pairs. The gap is what rollingMSE/MAE can actually score.
    const size_t matched = std::min(nextToFill_, predictionContainer_.size());
    const size_t outstanding = predictionContainer_.size() - matched;

    const std::string identity = std::format("ModelSession [{:s}, observed={}, outstanding={}]",
                                             *model_,
                                             observationCount_,
                                             outstanding);
    if (spec.mode == fmt::FormatMode::Identity) return identity;

    std::string out = identity;
    out += '\n';

    fmt::Table table({"property", "value"}, {fmt::Table::Align::Left, fmt::Table::Align::Right});
    table.addRow({"model", model_->name()});
    table.addRow({"context window", std::format("{}", windowSize_)});
    table.addRow({"tick", fmt::formatDuration(deltaT_)});
    table.addRow({"last actual", std::format("{}", fmt::AsDateTime{lastActualTimeStamp_})});
    table.addRule();
    table.addRow({"observations", std::format("{}", observationCount_)});
    table.addRow({"predictions tracked", std::format("{}", predictionContainer_.size())});
    table.addRow({"matched to actuals", std::format("{}", matched)});
    table.addRow({"awaiting actuals", std::format("{}", outstanding)});
    table.addRow({"unflushed writes", std::format("{}/{}", writeBuffer_.size(), writeBufferCapacity_)});
    table.addRule();

    // Lifetime figures divide the running sums; the rolling pair only looks at the tracking
    // window, so the two disagree once the deque has rotated — which is the point of showing
    // both, since a drift shows up in the rolling numbers first.
    const auto count = static_cast<double>(observationCount_);
    table.addRow({"MSE (lifetime)",
                  observationCount_ == 0 ? "N/A" : fmt::formatDouble(runningSumSquaredError_ / count, spec.precision)});
    table.addRow({"MAE (lifetime)",
                  observationCount_ == 0 ? "N/A" : fmt::formatDouble(runningSumAbsoluteError_ / count, spec.precision)});
    table.addRow({std::format("MSE (last {})", errorTrackingWindowSize_),
                  matched == 0 ? "N/A" : fmt::formatDouble(rollingMSE(errorTrackingWindowSize_), spec.precision)});
    table.addRow({std::format("MAE (last {})", errorTrackingWindowSize_),
                  matched == 0 ? "N/A" : fmt::formatDouble(rollingMAE(errorTrackingWindowSize_), spec.precision)});

    out += table.render();
    return out;
}

void ModelSession::println(const fmt::FormatSpec& spec) const { std::println("{}", toString(spec)); }

void ModelSession::flush_() {
    if (writeBuffer_.empty()) return;

    Timestamps timestamps;
    std::vector<double> values;
    timestamps.reserve(writeBuffer_.size());
    values.reserve(writeBuffer_.size());
    for (const auto& [ts, val] : writeBuffer_) {
        timestamps.push_back(ts);
        values.push_back(val);
    }

    SeriesKey key{model_->getViewTimeSeriesId(), deltaT_};
    TimeSeries ts(key.SeriesId, std::move(timestamps), std::move(values));

    try {
        context_.saver_->merge(key, ts);
    } catch (...) {
        logging::error("Could not Save to the repository");
        return;
    }
    writeBuffer_.clear();
}
}  // namespace ts
