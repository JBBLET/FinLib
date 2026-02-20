// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "core/TimeSeriesView.hpp"
#include <vector>
#include <utility>

using std::vector;
using std::shared_ptr;
using std::make_shared;
using std::move;

TimeSeries TimeSeriesView::operator+(const double& scalar) {
    std::vector<double> result;
    result.reserve(length_);

    for (size_t i = 0; i < length_; ++i) {
       result.push_back((*this)[i] + scalar);
    }

    return TimeSeries(
        source_->get_shared_timestamps(),
        std::move(result))
}

TimeSeries TimeSeriesView::operator-(const double& scalar) {
    std::vector<double> result;
    result.reserve(length_);

    for (size_t i = 0; i < length_; ++i) {
       result.push_back((*this)[i] - scalar);
    }

    return TimeSeries(
        source_->get_shared_timestamps(),
        std::move(result))
}

TimeSeries TimeSeriesView::operator*(const double& scalar) {
    std::vector<double> result;
    result.reserve(length_);

    for (size_t i = 0; i < length_; ++i) {
       result.push_back((*this)[i] * scalar);
    }

    return TimeSeries(
        source_->get_shared_timestamps(),
        std::move(result))
}

TimeSeries TimeSeriesView::operator+(const TimeSeriesView& other) const {
    if (!is_aligned_with(other))
        throw std::runtime_error("TimeSeries not aligned");

    std::vector<double> result;
    result.reserve(length_);

    for (size_t i = 0; i < length_; ++i) {
       result.push_back((*this)[i] + other[i]);
    }

    return TimeSeries(
        source_->get_shared_timestamps(),
        std::move(result))
}

TimeSeries TimeSeriesView::operator-(const TimeSeriesView& other) const {
    if (!is_aligned_with(other))
        throw std::runtime_error("TimeSeries not aligned");

    std::vector<double> result;
    result.reserve(length_);

    for (size_t i = 0; i < length_; ++i) {
       result.push_back((*this)[i] - other[i]);
    }

    return TimeSeries(
        source_->get_shared_timestamps(),
        std::move(result))
}

TimeSeries TimeSeriesView::operator*(const TimeSeriesView& other) const {
    if (!is_aligned_with(other))
        throw std::runtime_error("TimeSeries not aligned");

    std::vector<double> result;
    result.reserve(length_);

    for (size_t i = 0; i < length_; ++i) {
       result.push_back((*this)[i] * other[i]);
    }

    return TimeSeries(
        source_->get_shared_timestamps(),
        std::move(result))
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

vector<int64_t>&& getCopyTimestampsInView() const {
    original_timestamps = (source_->get_timestamps());
    vector<int64_t> sliced_data(original_timestamps.begin() + begin_, original_vec.begin() + begin_ + length_);
    return sliced_data;
}
