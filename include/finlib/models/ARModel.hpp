// Copyright 2026 JBBLET
#pragma once

#include <cstddef>
#include <format>
#include <string>

#include "Eigen/Core"
#include "Eigen/Dense"
#include "finlib/models/interfaces/BaseModel.hpp"

namespace models {

class ARModel : public BaseModel {
 public:
    enum class Solver { OLS, YuleWalker, LevinsonDurbin };
    explicit ARModel(size_t q, ARModel::Solver solver = ARModel::Solver::YuleWalker, double regularityTolerance = 0.2)
        : q_(q), solver_(solver), regularityTolerance_(regularityTolerance) {}

    std::string name() const override { return std::format("AR ({})", q_); };
    // std::string print() const override;

    void fit() override;
    EvaluationResult evaluate(const TimeSeriesView& view) override;
    double predictOneStep(const Eigen::VectorXd& window) const override;

    bool requiresRegularSpacing() const override { return true; }
    double regularityTolerance() const override { return regularityTolerance_; }

    bool isStationary() const;

 private:
    // Parameters
    double regularityTolerance_;
    Solver solver_;

    // Models properties
    size_t q_;
    Eigen::VectorXd phi_;
    double intercept_;
    double sigmaEpsilon_;
    Eigen::MatrixXd covarianceMatrix_;
    // Methods
    void yuleWalkerSolver_();
    void leastSquareSolver_(const Eigen::MatrixXd& X, const Eigen::VectorXd& Y);
    void levinsonDurbinSolver_();
};
}  // namespace models
