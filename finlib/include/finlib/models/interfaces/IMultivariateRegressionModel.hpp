// Copyright 2026 JBBLET
#pragma once
#include <memory>
#include <vector>

#include "Eigen/Dense"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/models/interfaces/EvaluationResult.hpp"
#include "finlib/models/interfaces/IModel.hpp"

namespace models {

class IMultivariateRegressionModel : public virtual IModel {
 public:
    virtual void setData(const std::vector<TimeSeriesView>& views, double trainRatio = 0.7,
                         double validationRatio = 0.15) = 0;
    virtual Eigen::VectorXd predict(const Eigen::MatrixXd& windows) const = 0;
    virtual RegressionEvaluation evaluate(const std::vector<TimeSeriesView>& views) = 0;
    virtual std::unique_ptr<IMultivariateRegressionModel> createFresh() const = 0;
};

}  // namespace models
