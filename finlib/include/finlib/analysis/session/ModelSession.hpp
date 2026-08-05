// Copyright 2026 JBBLET
#pragma once

#include <cstddef>
#include <deque>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "finlib/analysis/models/interfaces/IRegressionModel.hpp"
#include "finlib/analysis/session/AppContext.hpp"
#include "finlib/common/Error.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/Format.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"

namespace ts {
class ModelSession {
    AppContext& context_;

    std::shared_ptr<models::IRegressionModel> model_;
    Eigen::VectorXd window_;
    size_t windowSize_;

    // Prediction
    struct PredictionEntry {
        Timestamp timestamp;
        double predictedValue;
        std::optional<double> actualValue;
    };

    std::deque<PredictionEntry> predictionContainer_;
    std::vector<std::pair<Timestamp, double>> writeBuffer_;
    size_t writeBufferCapacity_ = 100;

    // Running Error Tracking
    size_t errorTrackingWindowSize_;
    double runningSumSquaredError_ = 0.0;
    double runningSumAbsoluteError_ = 0.0;
    size_t observationCount_ = 0;

    Timestamp lastActualTimeStamp_;
    Timestamp deltaT_;
    double deltaTTolerance_;

 public:
    ModelSession(AppContext& context, std::shared_ptr<models::IRegressionModel> model, const TimeSeriesView& view,
                 size_t errorTrackingWindowSize, Timestamp deltaT, double deltaTTolerance)
        : context_(context),
          model_(std::move(model)),
          errorTrackingWindowSize_(errorTrackingWindowSize),
          deltaT_(deltaT),
          deltaTTolerance_(deltaTTolerance) {
        ensure(model_->isFitted(), "Model used for session not Fitted");
        size_t viewLength = view.size();
        ensure(viewLength >= 1, "View passed in model session cannot be empty");
        windowSize_ = model_->contextSize();
        window_ = view.asEigenVector().tail(windowSize_);
        lastActualTimeStamp_ = view.timestamp(viewLength - 1);
    }
    ~ModelSession() { flush_(); }
    ModelSession(const ModelSession&) = delete;
    ModelSession& operator=(const ModelSession&) = delete;
    ModelSession(ModelSession&&) = delete;
    ModelSession& operator=(ModelSession&&) = delete;

    std::vector<PredictionEntry> forecast(size_t steps);
    void observe(double value, Timestamp timestamp);
    double rollingMSE(size_t lastN) const;
    double rollingMAE(size_t lastN) const;
    bool shouldRefit(double mseTreshold) const;

    void refit(const TimeSeriesView& newView);

    // Display — running error, how much of the prediction buffer has been matched against
    // actuals, and how many observations are still waiting to be flushed to the repository.
    std::string toString(const fmt::FormatSpec& spec = {}) const;
    void println(const fmt::FormatSpec& spec = {.mode = fmt::FormatMode::Describe}) const;

 private:
    // Helper
    size_t nextToFill_ = 0;
    void flush_();
};
}  // namespace ts

template <>
struct std::formatter<ts::ModelSession> {
    ts::fmt::FormatSpec spec;

    constexpr auto parse(std::format_parse_context& ctx) { return ts::fmt::parseFormatSpec(ctx, spec); }

    auto format(const ts::ModelSession& session, std::format_context& ctx) const -> std::format_context::iterator {
        return std::format_to(ctx.out(), "{}", session.toString(spec));
    }
};
