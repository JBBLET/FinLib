// Copyright 2026 JBBLET
#include "finlib/analysis/models/interfaces/EvaluationResult.hpp"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <format>
#include <numbers>
#include <numeric>
#include <optional>
#include <print>
#include <string>
#include <utility>
#include <vector>

#include "finlib/common/Error.hpp"
#include "finlib/common/Format.hpp"

namespace ts {
void models::RegressionEvaluation::computeRegressionMetrics(const std::vector<double>& actual,
                                                            const std::vector<double>& prediction,
                                                            const int& numberParameters, const double& sigmaEpsilon) {
    size_t nActual = actual.size(), nPrediction = prediction.size();
    const double* actualData = actual.begin().base();
    const double* predictionData = prediction.begin().base();
    ensure(nActual == nPrediction, "predicted ({}) and Actual ({}) vector have different size", nPrediction, nActual);
    ensure(nActual != 0, "No Data to compute Model Regression Evaluation Result");
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

std::string models::RegressionEvaluation::toString(const fmt::FormatSpec& spec) const {
    if (spec.mode == fmt::FormatMode::Identity) {
        return std::format("RegressionEvaluation[R2={}, RMSE={}]",
                           fmt::naOr(rSquared, spec.precision),
                           fmt::naOr(rmse, spec.precision));
    }

    // Errors and goodness-of-fit live on unrelated scales, so each is quoted on its own
    // terms instead of forcing one precision across the whole table.
    auto cell = [&](const std::optional<double>& v) { return fmt::naOr(v, spec.precision); };

    fmt::Table table({"metric", "value"}, {fmt::Table::Align::Left, fmt::Table::Align::Right});
    table.addRow({"MSE", cell(mse)});
    table.addRow({"RMSE", cell(rmse)});
    table.addRow({"MAE", cell(mae)});
    table.addRule();
    table.addRow({"R^2", cell(rSquared)});
    table.addRow({"adjusted R^2", cell(adjustedRSquared)});
    table.addRule();
    table.addRow({"log-likelihood", cell(logLikelihood)});
    table.addRow({"AIC", cell(aic)});

    return "RegressionEvaluation\n" + table.render();
}

void models::RegressionEvaluation::println(const fmt::FormatSpec& spec) const { std::println("{}", toString(spec)); }

void models::RegressionEvaluation::print() const { println(); }

std::string models::ClassificationEvaluation::toString(const fmt::FormatSpec& spec) const {
    const std::string identity = std::format("ClassificationEvaluation[accuracy={}, F1={}]",
                                             fmt::formatDouble(accuracy, spec.precision),
                                             fmt::formatDouble(f1Score, spec.precision));
    if (spec.mode == fmt::FormatMode::Identity) return identity;

    // Rates all sit in [0, 1]; four decimals is the readable resolution regardless of data.
    // Named `decimals` rather than `precision` — that one is a metric on this struct.
    const int decimals = spec.precision >= 0 ? spec.precision : 4;

    fmt::Table metrics({"metric", "value"}, {fmt::Table::Align::Left, fmt::Table::Align::Right});
    metrics.addRow({"accuracy", fmt::formatDouble(accuracy, decimals)});
    metrics.addRow({"precision", fmt::formatDouble(precision, decimals)});
    metrics.addRow({"recall", fmt::formatDouble(recall, decimals)});
    metrics.addRow({"F1", fmt::formatDouble(f1Score, decimals)});

    std::string out = "ClassificationEvaluation\n";
    out += metrics.render();

    const auto classes = static_cast<std::size_t>(confusionMatrix.rows());
    if (classes == 0 || confusionMatrix.cols() != confusionMatrix.rows()) return out;

    // Rows are the actual class, columns the predicted one — stated in the header because
    // the transpose is just as plausible a reading and silently inverts precision/recall.
    std::vector<std::string> headers{"actual \\ pred"};
    std::vector<fmt::Table::Align> alignment{fmt::Table::Align::Left};
    for (std::size_t c = 0; c < classes; ++c) {
        headers.push_back(std::format("{}", c));
        alignment.push_back(fmt::Table::Align::Right);
    }

    fmt::Table matrix(std::move(headers), std::move(alignment));
    for (std::size_t r = 0; r < classes; ++r) {
        std::vector<std::string> row{std::format("{}", r)};
        for (std::size_t c = 0; c < classes; ++c) {
            row.push_back(
                std::format("{}", confusionMatrix(static_cast<Eigen::Index>(r), static_cast<Eigen::Index>(c))));
        }
        matrix.addRow(std::move(row));
    }

    out += '\n';
    out += matrix.render();
    return out;
}

void models::ClassificationEvaluation::println(const fmt::FormatSpec& spec) const {
    std::println("{}", toString(spec));
}

void models::ClassificationEvaluation::print() const { println(); }
}  // namespace ts
