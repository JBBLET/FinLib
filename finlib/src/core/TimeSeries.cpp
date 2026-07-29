// Copyright 2026 JBBLET
#include "finlib/core/TimeSeries.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "finlib/common/Exception.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/core/TimeSeriesView.hpp"

namespace ts {

// ---------------------------------------------------------------------------
// constructor
// ---------------------------------------------------------------------------
TimeSeries::TimeSeries() : id_{""}, timestamps_{nullptr}, values_{} {}

TimeSeries::TimeSeries(std::string id, Timestamps ts, std::vector<double> vals)
    : id_(id), timestamps_(std::make_shared<const Timestamps>(std::move(ts))), values_(std::move(vals)) {
    if (timestamps_->size() != values_.size()) {
        throw std::invalid_argument("Size mismatch between timestamps and values");
    }
}

TimeSeries::TimeSeries(std::string id, TimestampsPtr ts, std::vector<double> vals)
    : id_(id), timestamps_(std::move(ts)), values_(std::move(vals)) {
    if (timestamps_->size() != values_.size()) {
        throw std::invalid_argument("Size mismatch between timestamps and values");
    }
}

TimeSeries::TimeSeries(std::string id, TimestampsPtr sharedTimestamps, size_t tsOffset, std::vector<double> vals)
    : id_(std::move(id)), timestamps_(std::move(sharedTimestamps)), tsOffset_(tsOffset), values_(std::move(vals)) {
    if (tsOffset_ + values_.size() > timestamps_->size()) {
        throw std::invalid_argument("TimeSeries: tsOffset + size exceeds timestamp vector length");
    }
}

// ---------------------------------------------------------------------------
// Named Factory
// ---------------------------------------------------------------------------
TimeSeries TimeSeries::synthetic(std::string id, TimestampsPtr ts, std::vector<double> vals) {
    TimeSeries s(std::move(id), std::move(ts), std::move(vals));
    s.isSynthetic_ = true;
    return s;
}

// Timestamps Accessors
size_t TimeSeries::lowerBound(Timestamp ts) const {
    auto span = getTimestamps();
    return std::distance(span.begin(), std::lower_bound(span.begin(), span.end(), ts));
}
size_t TimeSeries::upperBound(Timestamp ts) const {
    auto span = getTimestamps();
    return std::distance(span.begin(), std::upper_bound(span.begin(), span.end(), ts));
}
std::optional<double> TimeSeries::exactValue(Timestamp ts) const {
    size_t idx = lowerBound(ts);
    auto span = getTimestamps();
    if (idx < values_.size() && span[idx] == ts) return values_[idx];
    return std::nullopt;
}
double TimeSeries::latestValue(Timestamp ts) const {
    if (values_.empty()) throw std::runtime_error("TimeSeries::latestValue: empty series '" + id_ + "'");
    size_t idx = lowerBound(ts);
    if (idx == 0) throw std::runtime_error("TimeSeries::latestValue: ts before start of series '" + id_ + "'");
    auto span = getTimestamps();
    if (idx < values_.size() && span[idx] == ts) return values_[idx];
    return values_[idx - 1];
}

// ---------------------------------------------------------------------------
// Operator Overloading
// ---------------------------------------------------------------------------

// Operator *
TimeSeries& TimeSeries::operator*=(const TimeSeries& other) {
    verifyAlignment_(other);
    for (size_t i = 0; i < values_.size(); ++i) {
        values_[i] *= other.values_[i];
    }
    return *this;
}

TimeSeries TimeSeries::operator*(const TimeSeries& other) const {
    TimeSeries temp = TimeSeries{*this};
    temp *= other;
    return temp;
}

TimeSeries& TimeSeries::operator*=(double scalar) {
    for (double& v : values_) {
        v *= scalar;
    }
    return *this;
}

TimeSeries TimeSeries::operator*(double scalar) const {
    TimeSeries temp = TimeSeries{*this};
    temp *= scalar;
    return temp;
}

// Operator /
TimeSeries& TimeSeries::operator/=(const TimeSeries& other) {
    verifyAlignment_(other);
    for (size_t i = 0; i < values_.size(); ++i) {
        other.values_[i] == 0.0 ? values_[i] = 0.0 : values_[i] /= other.values_[i];
    }
    return *this;
}

TimeSeries TimeSeries::operator/(const TimeSeries& other) const {
    TimeSeries temp = TimeSeries{*this};
    temp /= other;
    return temp;
}

TimeSeries& TimeSeries::operator/=(double scalar) {
    if (scalar == 0.0) throw Exception(std::format("Division by 0 of TimeSeries {}", id_));
    for (double& v : values_) {
        v /= scalar;
    }
    return *this;
}

TimeSeries TimeSeries::operator/(double scalar) const {
    if (scalar == 0.0) throw Exception(std::format("Division by 0 of TimeSeries {}", id_));
    TimeSeries temp = TimeSeries{*this};
    temp /= scalar;
    return temp;
}

// Operator +
TimeSeries& TimeSeries::operator+=(const TimeSeries& other) {
    verifyAlignment_(other);
    for (size_t i = 0; i < values_.size(); ++i) {
        values_[i] += other.values_[i];
    }
    return *this;
}
TimeSeries TimeSeries::operator+(const TimeSeries& other) const {
    TimeSeries temp = TimeSeries{*this};
    temp += other;
    return temp;
}
TimeSeries& TimeSeries::operator+=(double scalar) {
    for (double& v : values_) {
        v += scalar;
    }
    return *this;
}
TimeSeries TimeSeries::operator+(double scalar) const {
    TimeSeries temp = TimeSeries{*this};
    temp += scalar;
    return temp;
}

// Operator -
TimeSeries& TimeSeries::operator-=(const TimeSeries& other) {
    verifyAlignment_(other);
    for (size_t i = 0; i < values_.size(); ++i) {
        values_[i] -= other.values_[i];
    }
    return *this;
}
TimeSeries TimeSeries::operator-(const TimeSeries& other) const {
    TimeSeries temp = TimeSeries{*this};
    temp -= other;
    return temp;
}
TimeSeries& TimeSeries::operator-=(double scalar) {
    for (double& v : values_) {
        v -= scalar;
    }
    return *this;
}
TimeSeries TimeSeries::operator-(double scalar) const {
    TimeSeries temp = TimeSeries{*this};
    temp -= scalar;
    return temp;
}

// ---------------------------------------------------------------------------
// Transformation Method
// ---------------------------------------------------------------------------
TimeSeriesView TimeSeries::view() const { return TimeSeriesView(shared_from_this(), 0, values_.size()); }

TimeSeriesView TimeSeries::slice(size_t start, size_t len) const {
    return TimeSeriesView(shared_from_this(), start, len);
}

TimeSeriesView TimeSeries::sliceIndex(size_t start, size_t end) const {
    return TimeSeriesView(shared_from_this(), start, end - start + 1);
}

// ---------------------------------------------------------------------------
// Private Helpers
// ---------------------------------------------------------------------------

void TimeSeries::verifyAlignment_(const TimeSeries& other) const {
    // Fast path: same backing vector at the same offset — definitely aligned.
    if (timestamps_ == other.timestamps_ && tsOffset_ == other.tsOffset_) return;
    if (this->size() != other.size()) {
        throw std::invalid_argument("TimeSeries size mismatch.");
    }
    // Slow path: compare only the slices each series actually represents.
    if (!std::equal(getTimestamps().begin(), getTimestamps().end(), other.getTimestamps().begin())) {
        throw std::invalid_argument("TimeSeries timestamps do not match.");
    }
}
}  // namespace ts
