// Copyright 2026 JBBLET
#pragma once
#include <memory>

#include "Eigen/Dense"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/models/interfaces/IModel.hpp"

namespace models {

struct PredictionDistribution {
    double mean;
    double variance;
};

class IProbabilisticModel : public virtual IModel {
 public:
    virtual void setData(const TimeSeriesView& view, double trainRatio = 0.7, double validationRatio = 0.15) = 0;
    virtual PredictionDistribution predictDistribution(const Eigen::VectorXd& window) const = 0;
    virtual std::unique_ptr<IProbabilisticModel> createFresh() const = 0;
};

}  // namespace models
