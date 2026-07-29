// Copyright 2026 JBBLET
#pragma once
#include <vector>

#include "Eigen/Dense"
#include "finlib/analysis/models/interfaces/EvaluationResult.hpp"
#include "finlib/analysis/models/interfaces/IModel.hpp"
#include "finlib/core/TimeSeriesView.hpp"

namespace ts::models {

class IClassificationModel : public virtual IModel {
 public:
    virtual void setData(const TimeSeriesView& view, const std::vector<int>& labels, double trainRatio = 0.7,
                         double validationRatio = 0.15) = 0;
    virtual int predictClass(const Eigen::VectorXd& features) const = 0;
    virtual Eigen::VectorXd predictProbabilities(const Eigen::VectorXd& features) const = 0;
    virtual ClassificationEvaluation evaluate(const TimeSeriesView& view, const std::vector<int>& labels) = 0;
};

}  // namespace ts::models
