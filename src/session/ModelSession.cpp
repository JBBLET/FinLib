// Copyright 2026 JBBLET

#include "finlib/session/ModelSession.hpp"

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "finlib/common/logger/LogMacros.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/data/SeriesKey.hpp"
#include "finlib/models/interfaces/IModel.hpp"

std::vector<ModelSession::PredictionEntry> ModelSession::forecast(size_t steps) {
    int64_t nextPredictedTimeStamp = lastActualTimeStamp_ + deltaT_;
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

void ModelSession::observe(double value, int64_t timestamp) {
    if (nextToFill_ >= predictionContainer_.size()) {
        return;  // or throw if this should never happen
    }

    PredictionEntry& entry = predictionContainer_[nextToFill_];

    if (std::abs(entry.timestamp - timestamp) > deltaTTolerance_) {
        LOG_WARN(context_, "Timestamp generated does not match any timestamp at which the actual value was received");
    }
    entry.actualValue = value;
    writeBuffer_.push_back(std::pair<int64_t, double>(timestamp, value));
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
    std::unique_ptr<models::IModel> newModel = model_->createFresh();
    newModel->setData(newData);
    newModel->fit();
    model_ = std::move(newModel);
    window_ = newData.asEigenVector().tail(windowSize_);
    lastActualTimeStamp_ = newData.timestamp(newData.size() - 1);
}

void ModelSession::flush_() {
    if (writeBuffer_.empty()) return;

    std::vector<int64_t> timestamps;
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
        context_.repository->merge(key, ts);
    } catch (...) {
        LOG_ERROR(context_, "Could not Save to the repository");
        return;
    }
    writeBuffer_.clear();
}
