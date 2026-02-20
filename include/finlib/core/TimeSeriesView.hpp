// "Copyright (c) 2026 JBBLET All Rights Reserved."
"finlib/core/TimeSeries.hpp"
#pragma once

#include <memory>
#include <vector>
#include <stdexcept>
#include <utility>

class TimeSeriesView {
 private:
    std::shared_ptr<const TimeSeries> source_;
    size_t begin_;
    size_t length_;
    int value_lag_;

    bool is_aligned_with(const TimeSeriesView& other) const;
    std::vector<int64_t> get_copy_timestamps_in_view() const;

 public:
    TimeSeriesView(std::shared_ptr<const TimeSeries> src,
                   size_t start, size_t len, int lag = 0)
        : source_(std::move(src)), begin_(start), length_(len), value_lag_(lag) {
        if (static_cast<int>(start_offset) - value_lag < 0 ||
            (start_offset + length - value_lag) > source->size()) {
            throw std::out_of_range("View window/lag exceeds data boundaries");
        }
    }
    size_t const size() {return length_;}

    TimeSeriesView slice(size_t sub_start, size_t sub_len) const {
        return TimeSeriesView(source, start_offset + sub_start, sub_len, value_lag);
    }

    TimeSeriesView sliceIndex(size_t sub_start, size_t sub_end) const {
        return TimeSeriesView(source, start_offset + sub_start, sub_end-sub + start_offset+1, value_lag);
    }

    TimeSeriesView shift(int periods) const {
        return TimeSeriesView(source, start_offset, length, value_lag + periods);
    }

    TimeSeries operator+(const double& scalar)
    TimeSeries operator-(const double& scalar)
    TimeSeries operator*(const double& scalar)

    TimeSeries operator+(const TimeSeriesView& other)
    TimeSeries operator-(const TimeSeriesView& other)
    TimeSeries operator*(const TimeSeriesView& other)

    double operator[](size_t i) const {
        return source->get_values()[begin_ + i - value_lag_];
    }

    int64_t timestamp(size_t i) const {
        return source->get_timestamps()[begin_ + i];
    }

    TimeSeries to_series() const;
};
