// Copyright 2026 JBBLET
#pragma once
#include <vector>

#include "Eigen/Dense"
#include "finlib/analysis/models/interfaces/EvaluationResult.hpp"
#include "finlib/analysis/models/interfaces/IModel.hpp"
#include "finlib/core/TimeSeriesView.hpp"

namespace ts::models {

class IMultivariateRegressionModel : public virtual IModel {
 public:
    virtual void setData(const std::vector<TimeSeriesView>& views, double trainRatio = 0.7,
                         double validationRatio = 0.15) = 0;
    virtual Eigen::VectorXd predict(const Eigen::MatrixXd& windows) const = 0;
    virtual RegressionEvaluation evaluate(const std::vector<TimeSeriesView>& views) = 0;
};

}  // namespace ts::models
