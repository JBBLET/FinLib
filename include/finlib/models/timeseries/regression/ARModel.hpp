// Copyright 2026 JBBLET
#pragma once

#include <cstddef>
#include <format>
#include <memory>
#include <string>

#include "Eigen/Core"
#include "Eigen/Dense"
#include "finlib/models/interfaces/BaseModel.hpp"

namespace models::regression {

class ARModel : public BaseModel {
 public:
    enum class Solver { OLS, YuleWalker, LevinsonDurbin };
    explicit ARModel(size_t q, ARModel::Solver solver = ARModel::Solver::YuleWalker, double regularityTolerance = 0.2)
        : q_(q), solver_(solver), regularityTolerance_(regularityTolerance) {
        phi_.resize(q_);
        standardErrors_.resize(q_ + 1);  // intercept + q coefficients
        tStatistics_.resize(q_ + 1);
        pValues_.resize(q_ + 1);
        covarianceMatrix_.resize(q_ + 1, q_ + 1);
    }

    ARModel(const ARModel&) = default;

    std::string name() const override { return std::format("AR ({})", q_); };
    // std::string print() const override;
    size_t contextSize() const override { return q_; };
    std::unique_ptr<IModel> createFresh() const override;

    // Setters and Getters
    bool requiresRegularSpacing() const override { return true; }
    double regularityTolerance() const override { return regularityTolerance_; }
    void setRegularityTolerance(double tolerance) {
        regularityTolerance_ = tolerance;
        if (!fullView_->checkRegularity(regularityTolerance_).isRegular) {
            isFitted_ = false;
        }
    }
    void setSolver(ARModel::Solver solver) {
        if (solver != solver_) {
            isFitted_ = false;
            solver_ = solver;
        }
    }

    // Interface implementation
    void fit() override;
    EvaluationResult evaluate(const TimeSeriesView& view) override;
    double predictOneStep(const Eigen::VectorXd& window) const override;
    bool isStationary() const;
    void clear();

 private:
    // Parameters
    size_t q_;
    double regularityTolerance_;
    Solver solver_;

    // Models properties
    Eigen::VectorXd phi_;
    double intercept_;
    double sigmaEpsilon_;
    Eigen::MatrixXd covarianceMatrix_;
    Eigen::VectorXd standardErrors_;
    Eigen::VectorXd tStatistics_;
    Eigen::VectorXd pValues_;
    // Methods
    void yuleWalkerSolver_();
    void leastSquareSolver_(const Eigen::MatrixXd& X, const Eigen::VectorXd& Y);
    void levinsonDurbinSolver_();
};
}  // namespace models::regression
