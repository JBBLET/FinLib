// Copyright 2026 JBBLET
#pragma once
#include <format>
#include <optional>
#include <string>
#include <vector>

#include "Eigen/Dense"
#include "finlib/common/Format.hpp"

namespace ts::models {

struct RegressionEvaluation {
    std::optional<double> mse;
    std::optional<double> rmse;
    std::optional<double> mae;
    std::optional<double> rSquared;
    std::optional<double> adjustedRSquared;
    std::optional<double> logLikelihood;
    std::optional<double> aic;

    // Metrics that were never computed print as N/A rather than being dropped: which ones
    // are missing says what computeRegressionMetrics could not establish (adjusted R^2
    // needs enough residual degrees of freedom, and everything is empty before a fit).
    std::string toString(const fmt::FormatSpec& spec = {}) const;
    void println(const fmt::FormatSpec& spec = {.mode = fmt::FormatMode::Describe}) const;
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

    // Describe mode follows the headline metrics with the confusion matrix laid out
    // actual-by-predicted, which is the orientation the counts are only readable in.
    std::string toString(const fmt::FormatSpec& spec = {}) const;
    void println(const fmt::FormatSpec& spec = {.mode = fmt::FormatMode::Describe}) const;
    void print() const;
};

}  // namespace ts::models

template <>
struct std::formatter<ts::models::RegressionEvaluation, char> {
    ts::fmt::FormatSpec spec;

    constexpr auto parse(std::format_parse_context& ctx) { return ts::fmt::parseFormatSpec(ctx, spec); }

    auto format(const ts::models::RegressionEvaluation& evaluation, std::format_context& ctx) const
        -> std::format_context::iterator {
        return std::format_to(ctx.out(), "{}", evaluation.toString(spec));
    }
};

template <>
struct std::formatter<ts::models::ClassificationEvaluation, char> {
    ts::fmt::FormatSpec spec;

    constexpr auto parse(std::format_parse_context& ctx) { return ts::fmt::parseFormatSpec(ctx, spec); }

    auto format(const ts::models::ClassificationEvaluation& evaluation, std::format_context& ctx) const
        -> std::format_context::iterator {
        return std::format_to(ctx.out(), "{}", evaluation.toString(spec));
    }
};
