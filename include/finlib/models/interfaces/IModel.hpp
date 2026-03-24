// Copyright 2026 JBBLET
#pragma once
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Eigen/Dense"
#include "finlib/core/TimeSeriesView.hpp"
class TimeSeriesView;

namespace models {
struct EvaluationResult {
    // Regression Metrics (Continuous values)
    std::optional<double> mse;               // Mean Squared Error
    std::optional<double> rmse;              // Root Mean Squared Error
    std::optional<double> mae;               // Mean Absolute Error
    std::optional<double> rSquared;          // Coefficient of Determination
    std::optional<double> adjustedRSquared;  // Adjusted Coeff of Determination
    // Statistical/Information Metrics
    std::optional<double> logLikelihood;
    std::optional<double> aic;  // Akaike Information Criterion (for model comparison)

    // Classification-like Metrics (Threshold-based)
    std::optional<double> precision;  // Accuracy of positive predictions
    std::optional<double> recall;     // Ability to find all positive instances
    std::optional<double> f1Score;

    void print() const;  // Formatted output

    void computeRegressionMetrics(const std::vector<double>& actual, const std::vector<double>& prediction,
                                  const int& numberParameters, const double& sigmaEpsilon);
};

class IModel : public std::enable_shared_from_this<IModel> {
 public:
    virtual ~IModel() = default;

    virtual std::string name() const = 0;
    virtual std::string print() const = 0;

    virtual void setData(const TimeSeriesView& view, double trainRatio = 0.7, double validationRatio = 0.15) = 0;
    virtual void fit() = 0;
    virtual EvaluationResult evaluate(const TimeSeriesView& view) = 0;
    virtual double predictOneStep(const Eigen::VectorXd& window) const = 0;

    virtual bool requiresRegularSpacing() const = 0;
    virtual double regularityTolerance() const = 0;
    bool isFitted() const { return isFitted_; }

    virtual size_t contextSize() const = 0;
    virtual std::unique_ptr<IModel> createFresh() const = 0;
    virtual std::string getViewTimeSeriesId() const = 0;

 protected:
    bool isFitted_ = false;
};
}  // namespace models
