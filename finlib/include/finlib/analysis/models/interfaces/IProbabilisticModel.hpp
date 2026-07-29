// Copyright 2026 JBBLET
#pragma once

#include "Eigen/Dense"
#include "finlib/analysis/models/interfaces/IModel.hpp"
#include "finlib/core/TimeSeriesView.hpp"

namespace ts::models {

struct PredictionDistribution {
    double mean;
    double variance;
};

class IProbabilisticModel : public virtual IModel {
 public:
    virtual void setData(const TimeSeriesView& view, double trainRatio = 0.7, double validationRatio = 0.15) = 0;
    virtual PredictionDistribution predictDistribution(const Eigen::VectorXd& window) const = 0;
};

}  // namespace ts::models
