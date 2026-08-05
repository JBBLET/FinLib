// Copyright 2026 JBBLET
#include "finlib/core/TimeSeries.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <iterator>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "finlib/common/Error.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/Format.hpp"
#include "finlib/core/TimeSeriesView.hpp"

namespace ts {

// ---------------------------------------------------------------------------
// constructor
// ---------------------------------------------------------------------------
TimeSeries::TimeSeries() : id_{""}, timestamps_{nullptr}, values_{} {}

TimeSeries::TimeSeries(std::string id, Timestamps ts, std::vector<double> vals)
    : id_(id), timestamps_(std::make_shared<const Timestamps>(std::move(ts))), values_(std::move(vals)) {
    ensure<InvalidArgument>(timestamps_->size() == values_.size(),
                            "Size mismatch between timestamps ({}) and values ({})",
                            timestamps_->size(),
                            values_.size());
}

TimeSeries::TimeSeries(std::string id, TimestampsPtr ts, std::vector<double> vals)
    : id_(id), timestamps_(std::move(ts)), values_(std::move(vals)) {
    ensure<InvalidArgument>(timestamps_->size() == values_.size(),
                            "Size mismatch between timestamps ({}) and values ({})",
                            timestamps_->size(),
                            values_.size());
}

TimeSeries::TimeSeries(std::string id, TimestampsPtr sharedTimestamps, size_t tsOffset, std::vector<double> vals)
    : id_(std::move(id)), timestamps_(std::move(sharedTimestamps)), tsOffset_(tsOffset), values_(std::move(vals)) {
    ensure<InvalidArgument>(tsOffset_ + values_.size() <= timestamps_->size(),
                            "TimeSeries: tsOffset ({}) + size ({}) exceeds timestamp vector length ({})",
                            tsOffset_,
                            values_.size(),
                            timestamps_->size());
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
    ensure(!values_.empty(), "TimeSeries::latestValue: empty series '{}'", id_);
    size_t idx = lowerBound(ts);
    ensure(idx != 0, "TimeSeries::latestValue: ts before start of series '{}'", id_);
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
    ensure(scalar != 0.0, "Division by 0 of TimeSeries {}", id_);
    for (double& v : values_) {
        v /= scalar;
    }
    return *this;
}

TimeSeries TimeSeries::operator/(double scalar) const {
    ensure(scalar != 0.0, "Division by 0 of TimeSeries {}", id_);
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
// Display
// ---------------------------------------------------------------------------

std::string TimeSeries::toString(const fmt::FormatSpec& spec) const {
    // A default-constructed series carries a null timestamp pointer, so getTimestamps()
    // is off limits until that is ruled out. This path runs inside failed ensure()
    // messages, where a second fault would bury the original one.
    const bool dated = timestamps_ != nullptr && !values_.empty();

    std::string identity = std::format("TimeSeries '{}'", id_);
    if (values_.empty()) {
        identity += " [empty";
    } else if (dated) {
        const auto span = getTimestamps();
        identity +=
            std::format(" [n={}, {} .. {}", values_.size(), fmt::AsDate{span.front()}, fmt::AsDate{span.back()});
    } else {
        identity += std::format(" [n={}, undated", values_.size());
    }
    if (isSynthetic_) identity += ", synthetic";
    identity += ']';

    switch (spec.mode) {
        case fmt::FormatMode::Identity:
            return identity;
        case fmt::FormatMode::Describe:
            return fmt::renderDescribe(identity, values_, spec.precision);
        default:
            return fmt::renderSeries(identity, dated ? getTimestamps() : std::span<const Timestamp>{}, values_, spec);
    }
}

void TimeSeries::println(const fmt::FormatSpec& spec) const { std::println("{}", toString(spec)); }

void TimeSeries::head(size_t rows) const { println({.mode = fmt::FormatMode::Head, .count = rows}); }

void TimeSeries::tail(size_t rows) const { println({.mode = fmt::FormatMode::Tail, .count = rows}); }

void TimeSeries::describe() const { println({.mode = fmt::FormatMode::Describe}); }

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
    // Both messages name the operands: which two series met is the first thing anyone asks,
    // and the identity form carries the id and the date span without dumping the values.
    ensure<InvalidArgument>(this->size() == other.size(), "TimeSeries size mismatch: {:s} vs {:s}", *this, other);
    // Slow path: compare only the slices each series actually represents.
    ensure<InvalidArgument>(std::equal(getTimestamps().begin(), getTimestamps().end(), other.getTimestamps().begin()),
                            "TimeSeries timestamps do not match: {:s} vs {:s}",
                            *this,
                            other);
}
}  // namespace ts
