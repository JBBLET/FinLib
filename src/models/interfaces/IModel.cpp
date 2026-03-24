// Copyright 2026 JBBLET
#include "finlib/models/interfaces/IModel.hpp"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <numeric>
#include <stdexcept>
#include <vector>

void models::EvaluationResult::computeRegressionMetrics(const std::vector<double>& actual,
                                                        const std::vector<double>& prediction,
                                                        const int& numberParameters, const double& sigmaEpsilon) {
    size_t nActual = actual.size(), nPrediction = prediction.size();
    const double* actualData = actual.begin().base();
    const double* predictionData = prediction.begin().base();
    if (nActual != nPrediction) throw std::runtime_error("predicted and Actual vector have different size");
    if (nActual == 0) throw std::runtime_error("No Data to compute Model Regression Evaluation Result");
    double sumSquaredErrors = 0.0, sumAbsoluteErrors = 0.0, M2 = 0.0;
    double avg = std::reduce(actual.begin(), actual.end()) / nActual;
    for (size_t i = 0; i < nActual; ++i) {
        M2 += (*actualData - avg) * (*actualData - avg);
        double residuals = (*actualData - *predictionData);
        sumSquaredErrors += residuals * residuals;
        sumAbsoluteErrors += std::abs(residuals);
        ++actualData;
        ++predictionData;
    }
    mse = sumSquaredErrors / nActual;
    rmse = std::sqrt(mse.value());
    mae = sumAbsoluteErrors / nActual;
    rSquared = 1 - sumSquaredErrors / M2;
    if (nActual != numberParameters - 1)
        adjustedRSquared = 1 - (1 - rSquared.value()) * ((static_cast<double>(nActual) - 1) /
                                                         ((static_cast<double>(nActual) - numberParameters - 1)));
    logLikelihood =
        -(static_cast<double>(nActual) / 2) * (std::log(2 * std::numbers::pi) + std::log(sigmaEpsilon * sigmaEpsilon)) -
        (sumSquaredErrors / (2 * sigmaEpsilon * sigmaEpsilon));
    aic = 2 * (numberParameters + 2) - 2 * logLikelihood.value();
}
