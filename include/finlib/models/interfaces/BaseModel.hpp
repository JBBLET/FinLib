// Copyright 2026 JBBLET
#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "finlib/analysis/TimeSeriesAnalysis.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/models/interfaces/IModel.hpp"

namespace models {

class BaseModel : public IModel {
 protected:
    // Every model will use these to access its assigned data
    std::shared_ptr<const TimeSeriesView> fullView_;
    TimeSeriesView trainView_;
    TimeSeriesView validationView_;
    TimeSeriesView testView_;

    // Status tracking

    EvaluationResult testModelEvaluationResult;
    std::optional<analysis::TimeSeriesAnalysis> trainAnalysis;

 public:
    BaseModel() = default;

    void setData(const TimeSeriesView& totalView, double trainRatio, double validationRatio) override {
        fullView_ = std::make_shared<const TimeSeriesView>(totalView);
        if (requiresRegularSpacing() && !fullView_->checkRegularity(regularityTolerance()).isRegular) {
            throw std::runtime_error("The Model requires a regularly sapced timeseries. Regularize manually");
        }
        size_t totalSize = fullView_->size();
        size_t trainSize = static_cast<size_t>(totalSize * trainRatio);
        size_t validationSize = static_cast<size_t>(totalSize * validationRatio);
        size_t testSize = totalSize - trainSize - validationSize;

        trainView_ = fullView_->slice(0, trainSize);
        trainAnalysis = trainAnalysis.emplace(trainView_);
        validationView_ = fullView_->slice(trainSize, validationSize);
        testView_ = fullView_->slice(trainSize + validationSize, testSize);
        isFitted_ = false;  // New data means we need to re-fit
    };

    std::string print() const override { return name() + "[Fitted: " + (isFitted_ ? "Yes" : "No") + "]"; };

    double regularityTolerance() const override { return 0.0; }
    std::string getViewTimeSeriesId() const override { return fullView_->getTimeSeriesId(); }
};
}  // namespace models
