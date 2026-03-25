// Copyright 2026 JBBLET
#pragma once
#include <memory>
#include <vector>

#include "Eigen/Dense"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/models/interfaces/EvaluationResult.hpp"
#include "finlib/models/interfaces/IModel.hpp"

namespace models {

class IClassificationModel : public virtual IModel {
 public:
    virtual void setData(const TimeSeriesView& view, const std::vector<int>& labels, double trainRatio = 0.7,
                         double validationRatio = 0.15) = 0;
    virtual int predictClass(const Eigen::VectorXd& features) const = 0;
    virtual Eigen::VectorXd predictProbabilities(const Eigen::VectorXd& features) const = 0;
    virtual ClassificationEvaluation evaluate(const TimeSeriesView& view, const std::vector<int>& labels) = 0;
    virtual std::unique_ptr<IClassificationModel> createFresh() const = 0;
};

}  // namespace models
