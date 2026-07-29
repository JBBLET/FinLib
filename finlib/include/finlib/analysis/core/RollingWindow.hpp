// Copyright 2026 JBBLET

#pragma once
#include <vector>

#include "Eigen/Core"

namespace ts::simulation {

class RollingWindow {
    Eigen::VectorXd w_;

 public:
    RollingWindow() = default;
    explicit RollingWindow(const std::vector<double>& seed)
        : w_(Eigen::Map<const Eigen::VectorXd>(seed.data(), static_cast<Eigen::Index>(seed.size()))) {}

    const Eigen::VectorXd& get() const { return w_; }
    Eigen::Index size() const { return w_.size(); }

    void push(double x) {
        const auto n = w_.size();
        if (n == 0) return;
        if (n > 1) w_.head(n - 1) = w_.tail(n - 1).eval();  // .eval(): segments alias
        w_(n - 1) = x;
    }
};
}  // namespace ts::simulation
