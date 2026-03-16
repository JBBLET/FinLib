// Copyright 2026 JBBLET
#pragma once

#include <cstddef>
#include <format>
#include <string>
#include <vector>

#include "Eigen/Core"
#include "Eigen/Dense"
#include "finlib/models/interfaces/BaseModel.hpp"

namespace models {

class ARModel : public BaseModel {
 public:
    enum class Solver { OLS, YuleWalker, LevinsonDurbin };
    explicit ARModel(size_t q, ARModel::Solver solver = ARModel::Solver::YuleWalker) : q_(q), solver_(solver) {}

    std::string name() const override { return std::format("AR ({})", q_); };

    void fit() override;
    EvaluationResult evaluate(const TimeSeriesView& view) const override;
    std::vector<double> predict_ts(const std::vector<int64_t> points) const override;
    std::vector<double> predict_points(int64_t steps) const override;

    bool is_stationary() const;

    std::string print() const override;

 private:
    Solver solver_;
    size_t q_;
    Eigen::VectorXd phi_;
    double intercept_;
    double sigma_epsilon_;
    Eigen::MatrixXd covariance_matrix_;
    Eigen::VectorXd last_window_;

    void yule_walker_solver_();
    void least_square_solver_(const Eigen::MatrixXd& X, const Eigen::VectorXd& Y);
    void levinson_durbin_solver_();
};
}  // namespace models
