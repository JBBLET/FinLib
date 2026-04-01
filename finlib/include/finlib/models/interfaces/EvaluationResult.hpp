// Copyright 2026 JBBLET
#pragma once
#include <optional>
#include <string>
#include <vector>

#include "Eigen/Dense"

namespace models {

struct RegressionEvaluation {
    std::optional<double> mse;
    std::optional<double> rmse;
    std::optional<double> mae;
    std::optional<double> rSquared;
    std::optional<double> adjustedRSquared;
    std::optional<double> logLikelihood;
    std::optional<double> aic;

    void print() const;

    void computeRegressionMetrics(const std::vector<double>& actual, const std::vector<double>& prediction,
                                  const int& numberParameters, const double& sigmaEpsilon);
};

struct ClassificationEvaluation {
    double accuracy = 0.0;
    double precision = 0.0;
    double recall = 0.0;
    double f1Score = 0.0;
    Eigen::MatrixXi confusionMatrix;

    void print() const;
};

}  // namespace models
