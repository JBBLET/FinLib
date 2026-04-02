// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <algorithm>
#include <execution>
#include <iostream>
#include <memory>
#include <optional>
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
    std::vector<double> values_;

    std::vector<double> partialWalk(const std::vector<int64_t>& targetTimestamps, size_t startIndex, size_t endIndex,
                                    InterpolationStrategy strategy, std::optional<uint32_t> seed) const;

 public:
    TimeSeries(std::string id, std::vector<int64_t> ts, std::vector<double> vals)
        : id_(id), timestamps_(std::make_shared<const std::vector<int64_t>>(std::move(ts))), values_(std::move(vals)) {
        if (timestamps_->size() != values_.size()) {
            throw std::invalid_argument("Size mismatch between timestamps and values");
        }
    }

    TimeSeries(std::string id, std::shared_ptr<const std::vector<int64_t>> ts, std::vector<double> vals)
        : id_(id), timestamps_(std::move(ts)), values_(std::move(vals)) {
        if (timestamps_->size() != values_.size()) {
            throw std::invalid_argument("Size mismatch between timestamps and values");
        }
    }

    TimeSeries(const TimeSeries& other)  // Can replace by default
        : id_(other.id_), timestamps_(other.timestamps_), values_(other.values_) {}

    friend void swap(TimeSeries& first, TimeSeries& second) noexcept {
        std::swap(first.id_, second.id_);
        std::swap(first.timestamps_, second.timestamps_);
        std::swap(first.values_, second.values_);
    }

    TimeSeries& operator=(TimeSeries other) noexcept {
        swap(*this, other);
        return *this;
    }

    TimeSeries(TimeSeries&& other) noexcept
        : id_(std::move(other.id_)), timestamps_(std::move(other.timestamps_)), values_(std::move(other.values_)) {}
    ~TimeSeries() = default;

    // Accessors
    size_t size() const { return values_.size(); }
    std::string getId() const { return id_; }
    const std::vector<double>& getValues() const { return values_; }
    const std::vector<int64_t>& getTimestamps() const { return *timestamps_; }
    const std::shared_ptr<const std::vector<int64_t>>& getSharedTimestamps() const { return timestamps_; }

    // helper
    void verifyAlignment(const TimeSeries& other) const;

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

    template <typename Func>
    TimeSeries apply(Func func) const& {
        // Create a new Time-series Object
        std::vector<double> new_vals(values_.size());
        if (values_.size() < 20000) {
            std::transform(values_.begin(), values_.end(), new_vals.begin(), func);
        } else {
            std::transform(std::execution::par, values_.begin(), values_.end(), new_vals.begin(), func);
        }
        return TimeSeries("Transformed " + id_, this->timestamps_, std::move(new_vals));
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
