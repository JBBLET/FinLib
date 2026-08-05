// Copyright 2026 JBBLET
#pragma once

#include <format>
#include <memory>
#include <string>

#include "finlib/analysis/models/interfaces/IRegressionModel.hpp"
#include "finlib/common/Error.hpp"
#include "finlib/analysis/seriesAnalysis/TimeSeriesAnalysis.hpp"
#include "finlib/core/TimeSeriesView.hpp"

namespace ts::models {

class BaseRegressionModel : public IRegressionModel {
 protected:
    std::shared_ptr<const TimeSeriesView> fullView_;
    TimeSeriesView trainView_;
    TimeSeriesView validationView_;
    TimeSeriesView testView_;

    RegressionEvaluation testModelEvaluationResult;
    std::optional<analysis::TimeSeriesAnalysis> trainAnalysis;

 public:
    BaseRegressionModel() = default;

    void setData(const TimeSeriesView& totalView, double trainRatio, double validationRatio) override {
        fullView_ = std::make_shared<const TimeSeriesView>(totalView);
        const auto regularity = fullView_->checkRegularity(regularityTolerance());
        ensure(!requiresRegularSpacing() || regularity.isRegular,
               "{} requires a regularly spaced timeseries; resample it or raise the tolerance. {} on {}",
               name(),
               regularity,
               totalView);
        size_t totalSize = fullView_->size();
        size_t trainSize = static_cast<size_t>(totalSize * trainRatio);
        size_t validationSize = static_cast<size_t>(totalSize * validationRatio);
        size_t testSize = totalSize - trainSize - validationSize;

        trainView_ = fullView_->slice(0, trainSize);
        trainAnalysis = trainAnalysis.emplace(trainView_);
        validationView_ = fullView_->slice(trainSize, validationSize);
        testView_ = fullView_->slice(trainSize + validationSize, testSize);
        isFitted_ = false;
    };

    // Default rendering for any regression model: identity plus how the data was split.
    // Concrete models override the describe modes to add their own parameter table.
    std::string toString(const fmt::FormatSpec& spec) const override {
        std::string identity = std::format("{} [{}", name(), isFitted_ ? "fitted" : "not fitted");
        if (fullView_ != nullptr) {
            identity += std::format(", train={}, validation={}, test={}",
                                    trainView_.size(),
                                    validationView_.size(),
                                    testView_.size());
        } else {
            identity += ", no data";
        }
        identity += ']';
        if (spec.mode == fmt::FormatMode::Identity || fullView_ == nullptr) return identity;
        return std::format("{}\non {:s}", identity, *fullView_);
    }

    double regularityTolerance() const override { return 0.0; }
    std::string getViewTimeSeriesId() const override { return fullView_->getTimeSeriesId(); }
};

}  // namespace ts::models
