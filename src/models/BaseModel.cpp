
#include "finlib/models/interfaces/BaseModel.hpp"

#include <string>

#include "finlib/core/TimeSeriesView.hpp"

void models::BaseModel::set_data(const TimeSeriesView& full_view, double train_p, double val_p) override {
    size_t total = full_view.size();
    size_t train_n = static_cast<size_t>(total * train_p);
    size_t val_n = static_cast<size_t>(total * val_p);
    size_t test_n = total - train_n - val_n;

    train_view_ = full_view.slice(0, train_n);
    val_view_ = full_view.slice(train_n, val_n);
    test_view_ = full_view.slice(train_n + val_n, test_n);
    is_fitted_ = false;  // New data means we need to re-fit
}

std::string models::BaseModel::print() const override {
    return name() + "[Fitted: " + (is_fitted_ ? "Yes" : "No") + "]";
}
