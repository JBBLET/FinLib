// Copyright 2026 JBBLET
#pragma once
#include <memory>
#include <string>

#include "Eigen/Dense"
#include "finlib/analysis/models/interfaces/EvaluationResult.hpp"
#include "finlib/analysis/models/interfaces/IModel.hpp"
#include "finlib/core/TimeSeriesView.hpp"

namespace ts::models {

class IRegressionModel : public virtual IModel {
 public:
    virtual void setData(const TimeSeriesView& view, double trainRatio = 0.7, double validationRatio = 0.15) = 0;
    virtual double predictOneStep(const Eigen::VectorXd& window) const = 0;
    virtual RegressionEvaluation evaluate(const TimeSeriesView& view) = 0;
    virtual std::string getViewTimeSeriesId() const = 0;

    // Non-virtual: an unfitted copy of this configuration, bound to new data and fitted.
    std::unique_ptr<IRegressionModel> refitted(const TimeSeriesView& view, double trainRatio = 0.7,
                                               double validationRatio = 0.15) const {
        auto fresh = createFreshAs<IRegressionModel>(*this);
        fresh->setData(view, trainRatio, validationRatio);
        fresh->fit();
        return fresh;
    }
};

}  // namespace ts::models
