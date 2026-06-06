// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <execution>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class TimeSeriesView;

enum class InterpolationStrategy { Linear, Stochastic, Nearest };
using TimestampPtr = std::shared_ptr<const std::vector<int64_t>>;

class TimeSeries : public std::enable_shared_from_this<TimeSeries> {
 private:
    std::string id_;
    TimestampPtr timestamps_;
    size_t tsOffset_ = 0;
    std::vector<double> values_;
    bool isSynthetic_ = false;

    // helper
    std::vector<double> partialWalk(const std::vector<int64_t>& targetTimestamps, size_t startIndex, size_t endIndex,
                                    InterpolationStrategy strategy, std::optional<uint32_t> seed) const;
    void verifyAlignment_(const TimeSeries& other) const;

 public:
    // Constructors
    TimeSeries(std::string id, std::vector<int64_t> ts, std::vector<double> vals);
    TimeSeries(std::string id, std::shared_ptr<const std::vector<int64_t>> ts, std::vector<double> vals);
    TimeSeries(std::string id, TimestampPtr sharedTimestamps, size_t tsOffset, std::vector<double> vals);

    TimeSeries(const TimeSeries& other)
        : id_(other.id_),
          timestamps_(other.timestamps_),
          tsOffset_(other.tsOffset_),
          values_(other.values_),
          isSynthetic_(other.isSynthetic_) {}

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
    size_t lowerBound(int64_t ts) const;
    size_t upperBound(int64_t ts) const;
    size_t tsOffset() const { return tsOffset_; }
    const std::shared_ptr<const std::vector<int64_t>>& getSharedTimestamps() const { return timestamps_; }
    std::span<const int64_t> getTimestamps() const {
        return {timestamps_->data() + tsOffset_, values_.size()};
    }  // Returns a span over the timestamps that belong to this series [tsOffset_, tsOffset_+size()).

    // Operator Overloading
    TimeSeries operator*(const TimeSeries& other) const;
    TimeSeries& operator*=(const TimeSeries& other);
    TimeSeries operator*(double scalar) const;
    TimeSeries& operator*=(double scalar);

    TimeSeries operator+(const TimeSeries& other) const;
    TimeSeries& operator+=(const TimeSeries& other);
    TimeSeries operator+(double scalar) const;
    TimeSeries& operator+=(double scalar);
    friend std::ostream& operator<<(std::ostream& os, const TimeSeries& obj);

    // Transformation Method
    TimeSeriesView view() const;
    TimeSeriesView slice(size_t start, size_t len) const;
    TimeSeriesView sliceIndex(size_t start, size_t end) const;
    TimeSeries resampling(const std::vector<int64_t>& targetTimestamps, InterpolationStrategy strategy,
                          std::optional<uint32_t> seed = std::nullopt) const;

    // Overload that shares the caller-owned timestamp vector — avoids an extra allocation
    // and lets downstream operators short-circuit alignment checks via pointer equality.
    TimeSeries resampling(TimestampPtr targetTimestamps, InterpolationStrategy strategy,
                          std::optional<uint32_t> seed = std::nullopt) const;

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
};

inline std::ostream& operator<<(std::ostream& os, const TimeSeries& obj) {
    const auto& ts = obj.getTimestamps();
    const auto& vs = obj.getValues();

    os << "[\n";
    for (size_t i = 0; i < obj.size(); ++i) {
        os << "  [" << ts[i] << ", " << vs[i] << "]\n";
    }
    os << "]";
    return os;
}
