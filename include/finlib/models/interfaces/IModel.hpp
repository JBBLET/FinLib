// Copyright 2026 JBBLET
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "finlib/core/TimeSeriesView.hpp"
class TimeSeriesView;

namespace models {
struct EvaluationResult {
    // Regression Metrics (Continuous values)
    double mse;        // Mean Squared Error
    double rmse;       // Root Mean Squared Error
    double mae;        // Mean Absolute Error
    double r_squared;  // Coefficient of Determination

    // Statistical/Information Metrics
    double log_likelihood;
    double aic;  // Akaike Information Criterion (for model comparison)

    // Classification-like Metrics (Threshold-based)
    // Useful if you're predicting "Direction" or "Outliers"
    double precision;  // Accuracy of positive predictions
    double recall;     // Ability to find all positive instances
    double f1_score;

    void print() const;  // Formatted output
};

class IModel {
 public:
    virtual ~IModel() = default;

    virtual void set_data(const TimeSeriesView& view, double train_ratio = 0.7, double val_ratio = 0.15) = 0;
    virtual void fit() = 0;

    virtual std::vector<double> predict_ts(const std::vector<int64_t> points) const = 0;
    virtual std::vector<double> predict_points(int64_t steps) const = 0;

    virtual EvaluationResult evaluate(const TimeSeriesView& view) const = 0;

    virtual std::string name() const = 0;

    virtual std::string print() const = 0;
};
}  // namespace models
