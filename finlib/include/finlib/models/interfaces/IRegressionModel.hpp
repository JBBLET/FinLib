// Copyright 2026 JBBLET
#pragma once
#include <memory>
#include <string>

#include "Eigen/Dense"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/models/interfaces/EvaluationResult.hpp"
#include "finlib/models/interfaces/IModel.hpp"

namespace models {

class IRegressionModel : public virtual IModel {
 public:
    virtual void setData(const TimeSeriesView& view, double trainRatio = 0.7, double validationRatio = 0.15) = 0;
    virtual double predictOneStep(const Eigen::VectorXd& window) const = 0;
    virtual RegressionEvaluation evaluate(const TimeSeriesView& view) = 0;
    virtual std::unique_ptr<IRegressionModel> createFresh() const = 0;
    virtual std::string getViewTimeSeriesId() const = 0;
};

}  // namespace models
