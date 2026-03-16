// Copyright 2026 JBBLET
#pragma once

#include <memory>
#include <string>

#include "finlib/analysis/TimeSeriesAnalysis.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/models/interfaces/IModel.hpp"

namespace models {

class BaseModel : public IModel {
 protected:
    // Every model will use these to access its assigned data

    std::shared_ptr<const TimeSeriesView> full_view;
    TimeSeriesView train_view_;
    TimeSeriesView val_view_;
    TimeSeriesView test_view_;

    // Status tracking
    bool is_fitted_ = false;

    analysis::TimeSeriesAnalysis train_analysis;

 public:
    explicit BaseModel() = default;
    void set_data(const TimeSeriesView& total_view, double train_p, double val_p) override {
        full_view = total_view.get_shared();
        size_t total = full_view->size();
        size_t train_n = static_cast<size_t>(total * train_p);
        size_t val_n = static_cast<size_t>(total * val_p);
        size_t test_n = total - train_n - val_n;

        train_view_ = full_view->slice(0, train_n);
        train_analysis = analysis::TimeSeriesAnalysis(train_view_);
        val_view_ = full_view->slice(train_n, val_n);
        test_view_ = full_view->slice(train_n + val_n, test_n);
        is_fitted_ = false;  // New data means we need to re-fit
    };

    std::string print() const override { return name() + "[Fitted: " + (is_fitted_ ? "Yes" : "No") + "]"; };
};
}  // namespace models
