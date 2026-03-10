// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <memory>
#include <vector>

#include "finlib/core/TimeSeries.hpp"

class TimeSeries;

class TimeSeriesView {
 private:
    std::shared_ptr<const TimeSeries> source_;
    size_t begin_;
    size_t length_;
    int value_lag_;

    bool is_aligned_with(const TimeSeriesView& other) const;
    std::shared_ptr<std::vector<int64_t>> get_copy_timestamps_in_view() const;

 public:
    const double* begin() const noexcept;

    const double* end() const noexcept;

    double operator[](size_t i) const;

    TimeSeriesView(std::shared_ptr<const TimeSeries> src, size_t start, size_t len, int lag = 0);

    size_t size() const noexcept { return length_; }

    TimeSeriesView slice(size_t sub_start, size_t sub_len) const {
        return TimeSeriesView(source_, begin_ + sub_start, sub_len, value_lag_);
    }

    TimeSeriesView slice_index(size_t sub_start, size_t sub_end) const {
        return TimeSeriesView(source_, begin_ + sub_start, sub_end - sub_start + 1, value_lag_);
    }

    TimeSeriesView shift(int periods) const { return TimeSeriesView(source_, begin_, length_, value_lag_ + periods); }

    TimeSeries operator+(const double& scalar) const;
    TimeSeries operator-(const double& scalar) const;
    TimeSeries operator*(const double& scalar) const;

    TimeSeries operator+(const TimeSeriesView& other) const;
    TimeSeries operator-(const TimeSeriesView& other) const;
    TimeSeries operator*(const TimeSeriesView& other) const;

    int64_t timestamp(size_t i) const;

    TimeSeries to_series() const;
};
