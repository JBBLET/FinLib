// Copyright 2026 JBBLET
#pragma once

#include <cmath>
#include <memory>
#include <random>

#include "finlib/common/Error.hpp"
#include "finlib/common/Random.hpp"

namespace ts::simulation {

// A standardized innovation: E[draw] == 0, Var[draw] == 1. Scale and drift belong to the process,
// not here, so a process can swap Gaussian for Student-t without touching its own arithmetic.
struct IInnovation {
    virtual ~IInnovation() = default;
    virtual double draw(Rng&) = 0;  // non-const: distributions cache state
    // std::normal_distribution caches a spare Box-Muller deviate, so sharing one across paths would
    // make results depend on evaluation order. Every path gets its own.
    virtual std::unique_ptr<IInnovation> clone() const = 0;
};

class GaussianInnovation final : public IInnovation {
    std::normal_distribution<double> z_{0.0, 1.0};

 public:
    double draw(Rng& g) override { return z_(g); }
    std::unique_ptr<IInnovation> clone() const override { return std::make_unique<GaussianInnovation>(); }
};

// Unit-variance Student-t: fatter tails than Gaussian at the same variance. nu > 2 is required for
// the variance to exist at all.
class StudentTInnovation final : public IInnovation {
    double nu_;
    double scale_;  // rescales to unit variance: t_nu has variance nu / (nu - 2)
    std::student_t_distribution<double> t_;

 public:
    explicit StudentTInnovation(double nu) : nu_(nu), scale_(std::sqrt((nu - 2.0) / nu)), t_(nu) {
        ensure<InvalidArgument>(nu > 2.0, "StudentTInnovation: nu must exceed 2 for finite variance, got {}", nu);
    }
    double draw(Rng& g) override { return t_(g) * scale_; }
    std::unique_ptr<IInnovation> clone() const override { return std::make_unique<StudentTInnovation>(nu_); }
};
}  // namespace ts::simulation
