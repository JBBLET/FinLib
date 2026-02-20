// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finlib/core/TimeSeriesView.hpp"
#include <vector>
#include <utility>
#include <memory>
#include "finlib/core/TimeSeries.hpp"



using std::vector;
using std::shared_ptr;
using std::make_shared;
using std::move;

TimeSeriesView::TimeSeriesView(std::shared_ptr<const TimeSeries> src,
                               size_t start, size_t len, int lag)
    : source_(std::move(src)), begin_(start), length_(len), value_lag_(lag) {
    if (static_cast<int>(begin_) - value_lag_ < 0 ||
        (begin_ + length_ - value_lag_) > source_->size()) {
        throw std::out_of_range("View window/lag exceeds data boundaries");
    }
}

double TimeSeriesView::operator[](size_t i) const {
    return source_->get_values()[begin_ + i - value_lag_];
}

int64_t TimeSeriesView::timestamp(size_t i) const {
    return source_->get_timestamps()[begin_ + i];
}

TimeSeries TimeSeriesView::operator+(const double& scalar) const {
    vector<double> result;
    result.reserve(length_);

    for (size_t i = 0; i < length_; ++i) {
       result.push_back((*this)[i] + scalar);
    }

    return TimeSeries(
        get_copy_timestamps_in_view(),
        move(result));
}

TimeSeries TimeSeriesView::operator-(const double& scalar) const {
    vector<double> result;
    result.reserve(length_);

    for (size_t i = 0; i < length_; ++i) {
       result.push_back((*this)[i] - scalar);
    }

    return TimeSeries(
        get_copy_timestamps_in_view(),
        move(result));
}

TimeSeries TimeSeriesView::operator*(const double& scalar) const {
    vector<double> result;
    result.reserve(length_);

    for (size_t i = 0; i < length_; ++i) {
       result.push_back((*this)[i] * scalar);
    }

    return TimeSeries(
        get_copy_timestamps_in_view(),
        move(result));
}

TimeSeries TimeSeriesView::operator+(const TimeSeriesView& other) const {
    if (!is_aligned_with(other))
        throw std::runtime_error("TimeSeries not aligned");

    vector<double> result;
    result.reserve(length_);

    for (size_t i = 0; i < length_; ++i) {
       result.push_back((*this)[i] + other[i]);
    }

    return TimeSeries(
        get_copy_timestamps_in_view(),
        move(result));
}

TimeSeries TimeSeriesView::operator-(const TimeSeriesView& other) const {
    if (!is_aligned_with(other))
        throw std::runtime_error("TimeSeries not aligned");

    vector<double> result;
    result.reserve(length_);

    for (size_t i = 0; i < length_; ++i) {
       result.push_back((*this)[i] - other[i]);
    }

    return TimeSeries(
        get_copy_timestamps_in_view(),
        move(result));
}

TimeSeries TimeSeriesView::operator*(const TimeSeriesView& other) const {
    if (!is_aligned_with(other))
        throw std::runtime_error("TimeSeries not aligned");

    vector<double> result;
    result.reserve(length_);

    for (size_t i = 0; i < length_; ++i) {
       result.push_back((*this)[i] * other[i]);
    }

    return TimeSeries(
        get_copy_timestamps_in_view(),
        move(result));
}

bool TimeSeriesView::is_aligned_with(const TimeSeriesView& other) const {
    if (source_->get_shared_timestamps() ==
        other.source_->get_shared_timestamps() &&
        begin_ == other.begin_ &&
        length_ == other.length_) {
        return true;
    } else if (length_ != other.length_) {
        return false;
    } else {
        const auto& ts1 = *source_->get_shared_timestamps();
        const auto& ts2 = *other.source_->get_shared_timestamps();

        for (size_t i = 0; i < length_; ++i) {
            if (ts1[begin_ + i] != ts2[other.begin_ + i])
                return false;
        }
    }
    return true;
}

shared_ptr<vector<int64_t>> TimeSeriesView::get_copy_timestamps_in_view() const {
    vector<int64_t> original_timestamps = (source_->get_timestamps());
    vector<int64_t> sliced_data(original_timestamps.begin() + begin_, original_timestamps.begin() + begin_ + length_);
    return make_shared<vector<int64_t>>(sliced_data);
}

TimeSeries TimeSeriesView::to_series() const {
    vector<double> result;
    result.reserve(length_);

    for (size_t i=0; i < length_; i++) {
        result.push_back((*this)[i]);
    }
    return TimeSeries(
            get_copy_timestamps_in_view(),
            move(result));
}
