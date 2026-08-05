// Copyright 2026 JBBLET
#include "finlib/analysis/models/timeseries/regression/ARModel.hpp"

#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "Eigen/Core"
#include "finlib/analysis/models/interfaces/EvaluationResult.hpp"
#include "finlib/common/Error.hpp"
#include "finlib/common/Format.hpp"
#include "finlib/core/StatsCore.hpp"

namespace ts::models::regression {
void ARModel::fit() {
    auto data = trainView_.asEigenVector();
    size_t n = data.size();
    ensure(n > q_, "Insufficient data points: need more than {}, got {}", q_, n);
    size_t rows = n - q_;

    using RowMajorMatrix = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    Eigen::Map<const RowMajorMatrix, Eigen::Unaligned, Eigen::Stride<1, 1>> lagMatrix(data.data(), rows, q_);

    Eigen::VectorXd Y = data.tail(rows);
    Eigen::MatrixXd X(rows, q_ + 1);
    X.col(0).setOnes();
    X.rightCols(q_) = lagMatrix;
    switch (solver_) {
        case ARModel::Solver::OLS:
            leastSquareSolver_(X, Y);
            break;
        case ARModel::Solver::YuleWalker:
            yuleWalkerSolver_();
            break;
        case ARModel::Solver::LevinsonDurbin:
            levinsonDurbinSolver_();
            break;
    }

    Eigen::VectorXd coeffs;
    coeffs.resize(q_ + 1);
    coeffs << intercept_, phi_.head(q_);

    Eigen::VectorXd residuals = Y - (X * coeffs);
    sigmaEpsilon_ = std::sqrt(residuals.squaredNorm() / (n - (q_ + 1)));
    Eigen::MatrixXd I = Eigen::MatrixXd::Identity(q_ + 1, q_ + 1);
    covarianceMatrix_ = (sigmaEpsilon_ * sigmaEpsilon_) * ((X.transpose() * X).ldlt().solve(I));
    standardErrors_ = covarianceMatrix_.diagonal().array().sqrt();
    tStatistics_ = coeffs.array() / standardErrors_.array();
    pValues_ =
        tStatistics_.unaryExpr([](double t) { return ts::analysis::hypothesisTesting::PvalueFromTStatistic(t); });
    isFitted_ = true;
    if (testView_.size() > q_) {
        // The result was previously computed and dropped on the floor, leaving
        // testModelEvaluationResult permanently empty — nothing could ever read the
        // out-of-sample metrics this line exists to produce.
        testModelEvaluationResult = evaluate(testView_);
    }
}

double ARModel::predictOneStep(const Eigen::VectorXd& window) const {
    ensure(isFitted_, "AR Model need to be fitted before predicting");
    double prediction = phi_.dot(window.reverse()) + intercept_;
    return prediction;
}

RegressionEvaluation ARModel::evaluate(const TimeSeriesView& view) {
    ensure(isFitted_, "AR Model need to be fitted before predicting");
    auto data = view.asEigenVector();
    size_t n = data.size();
    if (n < q_) return RegressionEvaluation{};
    std::vector<double> prediction;
    prediction.resize(n - q_);
    for (size_t i = q_; i < n; ++i) {
        auto window = data.segment(i - q_, q_);
        prediction[i - q_] = phi_.dot(window.reverse()) + intercept_;
    }
    std::vector<double> actual(data.data() + q_, data.data() + q_ + n - q_);
    RegressionEvaluation modelEvaluationResult;
    modelEvaluationResult.computeRegressionMetrics(actual, prediction, q_, sigmaEpsilon_);
    return modelEvaluationResult;
}

PredictionDistribution ARModel::predictDistribution(const Eigen::VectorXd& window) const {
    return {predictOneStep(window), sigmaEpsilon_};
}

void ARModel::yuleWalkerSolver_() {
    std::vector<double> gamma = trainAnalysis->autocovariances(q_);
    Eigen::MatrixXd R = trainAnalysis->toeplitz(q_);
    Eigen::VectorXd r(q_);
    for (int i = 0; i < q_; ++i) {
        r(i) = gamma[i + 1];
    }
    Eigen::VectorXd beta = R.ldlt().solve(r);
    intercept_ = (1 - std::accumulate(beta.begin(), beta.end(), 0.0)) * trainAnalysis->mean();
    phi_ = beta;
}

void ARModel::leastSquareSolver_(const Eigen::MatrixXd& X, const Eigen::VectorXd& Y) {
    Eigen::VectorXd beta = X.colPivHouseholderQr().solve(Y);
    intercept_ = beta(0);
    phi_ = beta.tail(q_).reverse();
}

void ARModel::levinsonDurbinSolver_() {
    std::vector<double> gamma = trainAnalysis->autocovariances(q_);
    Eigen::VectorXd phi(q_);
    Eigen::VectorXd phiPrevious(q_);

    double sigma = gamma[0];
    for (int k = 0; k < q_; ++k) {
        double sum = 0.0;
        for (int j = 0; j < k; ++j) {
            sum += phiPrevious[j] * gamma[k - j];
        }
        double lambda = (gamma[k + 1] - sum) / sigma;
        phi[k] = lambda;
        for (int j = 0; j < k; ++j) {
            phi[j] = phiPrevious[j] - lambda * phiPrevious[k - j - 1];
        }
        sigma *= (1.0 - lambda * lambda);
        phiPrevious = phi;
    }
    intercept_ = (1 - std::accumulate(phi.begin(), phi.end(), 0.0)) * trainAnalysis->mean();
    phi_ = phi;
}

bool ARModel::isStationary() const {
    if (!isFitted_) return false;
    Eigen::MatrixXd companion = Eigen::MatrixXd::Zero(q_, q_);
    companion.row(0) = phi_.transpose();
    // Identity matrix below the first row (shifted)
    if (q_ > 1) {
        companion.block(1, 0, q_ - 1, q_ - 1).setIdentity();
    }

    // Solve for eigenvalues
    Eigen::EigenSolver<Eigen::MatrixXd> solver(companion);
    auto eigenvalues = solver.eigenvalues();

    for (int i = 0; i < eigenvalues.size(); ++i) {
        if (std::abs(eigenvalues(i)) >= 1.0) {
            return false;  // Non-stationary / Unit Root
        }
    }
    return true;
}

std::string ARModel::toString(const fmt::FormatSpec& spec) const {
    std::string identity = std::format("{} [{}, solver={}", name(), isFitted_ ? "fitted" : "not fitted", solver_);
    if (isFitted_) identity += isStationary() ? ", stationary" : ", NON-STATIONARY";
    if (fullView_ != nullptr) {
        identity += std::format(
            ", train={}, validation={}, test={}", trainView_.size(), validationView_.size(), testView_.size());
    }
    identity += ']';

    if (spec.mode == fmt::FormatMode::Identity) return identity;

    // Before fit(), phi_ and the diagnostic vectors are sized but never written — Eigen's
    // resize does not value-initialise. Printing them would be printing whatever was on the
    // heap, so the table only exists once there is something real in it.
    if (!isFitted_) return identity + "\n(no coefficients: model has not been fitted)";

    std::string out = identity;
    out += '\n';

    // The intercept leads the diagnostic vectors, then the q lag coefficients — the same
    // ordering fit() assembles `coeffs` in.
    const auto terms = static_cast<Eigen::Index>(q_) + 1;
    std::vector<double> estimates;
    estimates.reserve(static_cast<size_t>(terms));
    estimates.push_back(intercept_);
    for (Eigen::Index i = 0; i < static_cast<Eigen::Index>(q_); ++i) estimates.push_back(phi_(i));

    const auto coefficient = fmt::columnFormat(estimates, spec.precision);

    fmt::Table table({"term", "coef", "std err", "t", "P>|t|"},
                     {fmt::Table::Align::Left,
                      fmt::Table::Align::Right,
                      fmt::Table::Align::Right,
                      fmt::Table::Align::Right,
                      fmt::Table::Align::Right});
    for (Eigen::Index i = 0; i < terms; ++i) {
        table.addRow({i == 0 ? std::string{"const"} : std::format("phi[{}]", i),
                      coefficient(estimates[static_cast<size_t>(i)]),
                      fmt::formatDouble(standardErrors_(i), 6),
                      fmt::formatDouble(tStatistics_(i), 4),
                      fmt::formatDouble(pValues_(i), 4)});
    }
    out += table.render();
    out += std::format("sigma(eps) = {}\n", fmt::formatDouble(sigmaEpsilon_, spec.precision));
    return out;
}

void ARModel::clear() {
    if (isFitted_) {
        intercept_ = 0.0;
        sigmaEpsilon_ = 0.0;
        phi_.setZero();
        standardErrors_.setZero();
        tStatistics_.setZero();
        pValues_.setZero();
        covarianceMatrix_.setZero();
        isFitted_ = false;
    }
}
std::unique_ptr<ts::models::IModel> ARModel::createFresh() const {
    return std::make_unique<ARModel>(q_, solver_, regularityTolerance_);
}
}  // namespace ts::models::regression
