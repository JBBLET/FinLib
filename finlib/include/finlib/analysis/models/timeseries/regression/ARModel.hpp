// Copyright 2026 JBBLET
#pragma once

#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <string_view>

#include "Eigen/Core"
#include "Eigen/Dense"
#include "finlib/analysis/models/interfaces/BaseRegressionModel.hpp"
#include "finlib/analysis/models/interfaces/IModel.hpp"
#include "finlib/analysis/models/interfaces/IProbabilisticModel.hpp"

namespace ts::models::regression {

class ARModel : public BaseRegressionModel, public IProbabilisticModel {
 public:
    enum class Solver { OLS, YuleWalker, LevinsonDurbin };

    static constexpr std::string_view toString(Solver solver) {
        switch (solver) {
            case Solver::OLS: return "OLS";
            case Solver::YuleWalker: return "YuleWalker";
            case Solver::LevinsonDurbin: return "LevinsonDurbin";
        }
        return "<unknown Solver>";
    }

    explicit ARModel(size_t q, ARModel::Solver solver = ARModel::Solver::YuleWalker, double regularityTolerance = 0.2)
        : q_(q), solver_(solver), regularityTolerance_(regularityTolerance) {
        phi_.resize(q_);
        standardErrors_.resize(q_ + 1);  // intercept + q coefficients
        tStatistics_.resize(q_ + 1);
        pValues_.resize(q_ + 1);
        covarianceMatrix_.resize(q_ + 1, q_ + 1);
    }

    // Constructors and Destructors
    ARModel(const ARModel&) = default;
    ARModel& operator=(const ARModel&) = default;
    ARModel(ARModel&&) = default;
    ARModel& operator=(ARModel&&) = default;
    ~ARModel() override = default;

    // IModel Interface
    std::string name() const override { return std::format("AR ({})", q_); };
    // Describe mode is the coefficient table: estimate, standard error, t and p per term.
    // All of it is already stored by fit(); it simply had no way out until now.
    std::string toString(const fmt::FormatSpec& spec) const override;
    bool requiresRegularSpacing() const override { return true; }
    double regularityTolerance() const override { return regularityTolerance_; }
    size_t contextSize() const override { return q_; };
    void fit() override;
    std::unique_ptr<ts::models::IModel> createFresh() const override;

    // IRegressionModel Interface
    double predictOneStep(const Eigen::VectorXd& window) const override;
    RegressionEvaluation evaluate(const TimeSeriesView& view) override;
    void setData(const TimeSeriesView& totalView, double trainRatio, double validationRatio) override {
        this->BaseRegressionModel::setData(totalView, trainRatio, validationRatio);
    };

    // IProbabilisticModel
    PredictionDistribution predictDistribution(const Eigen::VectorXd& window) const override;

    // ARModel Interface
    bool isStationary() const;
    void clear();

    // Setters and Getters
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
}  // namespace ts::models::regression

template <>
struct std::formatter<ts::models::regression::ARModel::Solver> : std::formatter<std::string_view> {
    auto format(ts::models::regression::ARModel::Solver solver, std::format_context& ctx) const
        -> std::format_context::iterator {
        return std::formatter<std::string_view>::format(ts::models::regression::ARModel::toString(solver), ctx);
    }
};
