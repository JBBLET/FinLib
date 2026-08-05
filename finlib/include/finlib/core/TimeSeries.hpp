// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <algorithm>
#include <cstddef>
#include <execution>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/Format.hpp"

namespace ts {
class TimeSeriesView;

class TimeSeries : public std::enable_shared_from_this<TimeSeries> {
 public:
    // Constructor
    TimeSeries();
    TimeSeries(std::string id, Timestamps ts, std::vector<double> vals);
    TimeSeries(std::string id, TimestampsPtr ts, std::vector<double> vals);
    TimeSeries(std::string id, TimestampsPtr sharedTimestamps, size_t tsOffset, std::vector<double> vals);

    TimeSeries(const TimeSeries& other)
        : id_(other.id_),
          timestamps_(other.timestamps_),
          tsOffset_(other.tsOffset_),
          values_(other.values_),
          isSynthetic_(other.isSynthetic_) {}

    static TimeSeries synthetic(std::string id, TimestampsPtr ts, std::vector<double> vals);

    friend void swap(TimeSeries& first, TimeSeries& second) noexcept {
        std::swap(first.id_, second.id_);
        std::swap(first.timestamps_, second.timestamps_);
        std::swap(first.tsOffset_, second.tsOffset_);
        std::swap(first.values_, second.values_);
        std::swap(first.isSynthetic_, second.isSynthetic_);
    }

    TimeSeries& operator=(TimeSeries other) noexcept {
        swap(*this, other);
        return *this;
    }

    TimeSeries(TimeSeries&& other) noexcept
        : id_(std::move(other.id_)),
          timestamps_(std::move(other.timestamps_)),
          tsOffset_(other.tsOffset_),
          values_(std::move(other.values_)),
          isSynthetic_(other.isSynthetic_) {}
    ~TimeSeries() = default;

    // Accessors
    size_t size() const { return values_.size(); }
    std::string getId() const { return id_; }
    const std::vector<double>& getValues() const { return values_; }
    bool isSynthetic() const { return isSynthetic_; }

    // TimeStamps Accessors
    size_t lowerBound(Timestamp ts) const;
    size_t upperBound(Timestamp ts) const;
    // Returns the value at exactly ts, or nullopt if no such timestamp exists.
    std::optional<double> exactValue(Timestamp ts) const;
    // Returns the value at the latest timestamp <= ts (look-back only, no look-ahead).
    // Throws if the series is empty or ts is before the first point.
    double latestValue(Timestamp ts) const;
    size_t tsOffset() const { return tsOffset_; }
    const TimestampsPtr& getSharedTimestamps() const { return timestamps_; }
    std::span<const Timestamp> getTimestamps() const {
        return {timestamps_->data() + tsOffset_, values_.size()};
    }  // Returns a span over the timestamps that belong to this series [tsOffset_, tsOffset_+size()).

    // Operator Overloading
    TimeSeries operator*(const TimeSeries& other) const;
    TimeSeries& operator*=(const TimeSeries& other);
    TimeSeries operator*(double scalar) const;
    TimeSeries& operator*=(double scalar);

    TimeSeries operator/(const TimeSeries& other) const;
    TimeSeries& operator/=(const TimeSeries& other);
    TimeSeries operator/(double scalar) const;
    TimeSeries& operator/=(double scalar);

    TimeSeries operator+(const TimeSeries& other) const;
    TimeSeries& operator+=(const TimeSeries& other);
    TimeSeries operator+(double scalar) const;
    TimeSeries& operator+=(double scalar);

    TimeSeries operator-(const TimeSeries& other) const;
    TimeSeries& operator-=(const TimeSeries& other);
    TimeSeries operator-(double scalar) const;
    TimeSeries& operator-=(double scalar);

    // Display
    // toString({}) is a single line — it is what lands in ensure() messages and log
    // lines, and it never touches the values. The multi-line modes are for the console.
    // None of these throw, including on a default-constructed (timestamp-less) series.
    std::string toString(const fmt::FormatSpec& spec = {}) const;
    void println(const fmt::FormatSpec& spec = {.mode = fmt::FormatMode::Repr}) const;
    void head(std::size_t rows = 10) const;
    void tail(std::size_t rows = 10) const;
    void describe() const;

    friend std::ostream& operator<<(std::ostream& os, const TimeSeries& obj);

    // Transformation Method
    TimeSeriesView view() const;
    TimeSeriesView slice(size_t start, size_t len) const;
    TimeSeriesView sliceIndex(size_t start, size_t end) const;

    template <typename Func>
    TimeSeries apply(Func func) const& {
        std::vector<double> new_vals(values_.size());
        if (values_.size() < 20000) {
            std::transform(values_.begin(), values_.end(), new_vals.begin(), func);
        } else {
            std::transform(std::execution::par, values_.begin(), values_.end(), new_vals.begin(), func);
        }
        // Share the parent's TimestampPtr and preserve tsOffset_ — zero timestamp allocation.
        return TimeSeries("Transformed " + id_, timestamps_, tsOffset_, std::move(new_vals));
    }

    template <typename Func>
    TimeSeries apply(Func func) && {
        if (values_.size() < 20000) {
            std::transform(values_.begin(), values_.end(), values_.begin(), func);
        } else {
            std::transform(std::execution::par, values_.begin(), values_.end(), values_.begin(), func);
        }
        return std::move(*this);
    }

    template <typename Func>
    TimeSeries& applyInPlace(Func func) {
        if (values_.size() < 20000) {
            std::transform(values_.begin(), values_.end(), values_.begin(), func);
        } else {
            std::transform(std::execution::par, values_.begin(), values_.end(), values_.begin(), func);
        }
        return *this;
    }

 private:
    std::string id_;
    TimestampsPtr timestamps_;
    size_t tsOffset_ = 0;
    std::vector<double> values_;
    bool isSynthetic_ = false;

    void verifyAlignment_(const TimeSeries& other) const;
};

inline std::ostream& operator<<(std::ostream& os, const TimeSeries& obj) {
    return os << obj.toString({.mode = fmt::FormatMode::Repr});
}
}  // namespace ts

template <>
struct std::formatter<ts::TimeSeries, char> {
    ts::fmt::FormatSpec spec;

    constexpr auto parse(std::format_parse_context& ctx) { return ts::fmt::parseFormatSpec(ctx, spec); }

    auto format(const ts::TimeSeries& series, std::format_context& ctx) const -> std::format_context::iterator {
        return std::format_to(ctx.out(), "{}", series.toString(spec));
    }
};
