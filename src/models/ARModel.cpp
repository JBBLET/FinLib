// Copyright 2026 JBBLET
#include "finlib/models/ARModel.hpp"

#include <cmath>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <vector>

#include "Eigen/Core"
namespace models {
void ARModel::fit() {
    auto data = train_view_.as_eigen_vector();
    size_t n = data.size();
    if (n <= q_) throw std::runtime_error("Insufficient data points");
    size_t rows = n - q_;

    using RowMajorMatrix = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    Eigen::Map<const RowMajorMatrix, Eigen::Unaligned, Eigen::Stride<1, 1>> lag_matrix(data.data(), rows, q_);

    Eigen::VectorXd Y = data.tail(rows);
    Eigen::MatrixXd X(rows, q_ + 1);
    X.col(0).setOnes();
    X.rightCols(q_) = lag_matrix;
    switch (solver_) {
        case ARModel::Solver::OLS:
            least_square_solver_(X, Y);
            break;
        case ARModel::Solver::YuleWalker:
            yule_walker_solver_();
            break;
        case models::ARModel::Solver::LevinsonDurbin:
            levinson_durbin_solver_();
            break;
    }
    Eigen::VectorXd coeffs;
    coeffs.resize(q_ + 1);
    coeffs << intercept_, phi_.head(q_);
    Eigen::VectorXd residuals = Y - (X * coeffs);
    sigma_epsilon_ = std::sqrt(residuals.squaredNorm() / (n - (q_ + 1)));
    is_fitted_ = true;
}

std::vector<double> ARModel::predict_ts(const std::vector<int64_t> points) const {}

std::vector<double> ARModel::predict_points(int64_t steps) const {
    Eigen::VectorXd X;
    X.resize(q_ + steps);
    auto data = test_view_.as_eigen_vector();
    X << data.tail(q_);
}

EvaluationResult ARModel::evaluate(const TimeSeriesView& view) const {}

void ARModel::yule_walker_solver_() {
    std::vector<double> gamma = train_analysis.autocovariances(q_);
    Eigen::MatrixXd R = train_analysis.toeplitz(q_);
    Eigen::VectorXd r(q_);
    for (int i = 0; i < q_; ++i) {
        r(i) = gamma[i + 1];
    }
    Eigen::VectorXd beta = R.ldlt().solve(r);
    intercept_ = (1 - std::accumulate(beta.begin(), beta.end(), 0.0)) * train_analysis.mean();
    phi_ = beta;
}

void ARModel::least_square_solver_(const Eigen::MatrixXd& X, const Eigen::VectorXd& Y) {
    Eigen::VectorXd beta = X.colPivHouseholderQr().solve(Y);
    intercept_ = beta(0);
    phi_ = beta.tail(q_);
}

void ARModel::levinson_durbin_solver_() {
    std::vector<double> gamma = train_analysis.autocovariances(q_);
    Eigen::VectorXd phi(q_);
    Eigen::VectorXd phi_prev(q_);

    double sigma = gamma[0];
    for (int k = 0; k < q_; ++k) {
        double sum = 0.0;

        for (int j = 0; j < k; ++j) sum += phi_prev[j] * gamma[k - j];

        double lambda = (gamma[k + 1] - sum) / sigma;

        phi[k] = lambda;

        for (int j = 0; j < k; ++j) phi[j] = phi_prev[j] - lambda * phi_prev[k - j - 1];

        sigma *= (1.0 - lambda * lambda);

        phi_prev = phi;
    }
    intercept_ = (1 - std::accumulate(phi.begin(), phi.end(), 0.0)) * train_analysis.mean();
    phi_ = phi;
}

bool ARModel::is_stationary() const {
    if (!is_fitted_) return false;

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
}  // namespace models
