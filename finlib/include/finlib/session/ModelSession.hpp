// Copyright 2026 JBBLET
#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/models/interfaces/IRegressionModel.hpp"
#include "finlib/session/AppContext.hpp"

class ModelSession {
    AppContext& context_;

    std::shared_ptr<models::IRegressionModel> model_;
    Eigen::VectorXd window_;
    size_t windowSize_;

    // Prediction
    struct PredictionEntry {
        int64_t timestamp;
        double predictedValue;
        std::optional<double> actualValue;
    };

    std::deque<PredictionEntry> predictionContainer_;
    std::vector<std::pair<int64_t, double>> writeBuffer_;
    size_t writeBufferCapacity_ = 100;

    // Running Error Tracking
    size_t errorTrackingWindowSize_;
    double runningSumSquaredError_ = 0.0;
    double runningSumAbsoluteError_ = 0.0;
    size_t observationCount_ = 0;

    int64_t lastActualTimeStamp_;
    int64_t deltaT_;
    double deltaTTolerance_;

 public:
    ModelSession(AppContext& context, std::shared_ptr<models::IRegressionModel> model, const TimeSeriesView& view,
                 size_t errorTrackingWindowSize, int64_t deltaT, double deltaTTolerance)
        : context_(context),
          model_(std::move(model)),
          errorTrackingWindowSize_(errorTrackingWindowSize),
          deltaT_(deltaT),
          deltaTTolerance_(deltaTTolerance) {
        if (!model_->isFitted()) throw std::runtime_error("Model used for session not Fitted");
        size_t viewLength = view.size();
        if (viewLength < 1) throw std::runtime_error("View passed in model session cannot be empty");
        windowSize_ = model_->contextSize();
        window_ = view.asEigenVector().tail(windowSize_);
        lastActualTimeStamp_ = view.timestamp(viewLength - 1);
    }
    ~ModelSession() { flush_(); }

    std::vector<PredictionEntry> forecast(size_t steps);
    void observe(double value, int64_t timestamp);
    double rollingMSE(size_t lastN) const;
    double rollingMAE(size_t lastN) const;
    bool shouldRefit(double mseTreshold) const;

    void refit(const TimeSeriesView& newView);

 private:
    // Helper
    size_t nextToFill_ = 0;
    void flush_();
};
