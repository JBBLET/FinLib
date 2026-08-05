// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finlib/core/TimeSeriesView.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
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
#include "finlib/core/TimeSeries.hpp"

using std::vector;
namespace ts {
TimeSeriesView::TimeSeriesView(std::shared_ptr<const TimeSeries> src, size_t start, size_t len, int lag)
    : source_(std::move(src)), begin_(start), length_(len), valueLag_(lag) {
    ensure(static_cast<int>(begin_) - valueLag_ >= 0 && (begin_ + length_ - valueLag_) <= source_->size(),
           "View window/lag exceeds data boundaries");
}

Timestamp TimeSeriesView::timestamp(size_t i) const { return source_->getTimestamps()[begin_ + i]; }

const double* TimeSeriesView::begin() const noexcept { return &(source_->getValues()[begin_ - valueLag_]); }

const double* TimeSeriesView::end() const noexcept { return begin() + length_; }

double TimeSeriesView::operator[](size_t i) const { return source_->getValues()[begin_ + i - valueLag_]; }

TimeSeries TimeSeriesView::materialise_(const std::string& id, std::vector<double> vals) const {
    return TimeSeries(id, source_->getSharedTimestamps(), source_->tsOffset() + begin_, std::move(vals));
}

TimeSeries TimeSeriesView::operator+(const double& scalar) const {
    vector<double> result;
    result.reserve(length_);
    for (size_t i = 0; i < length_; ++i) result.push_back((*this)[i] + scalar);
    return materialise_("ViewChange " + getTimeSeriesId(), std::move(result));
}

TimeSeries TimeSeriesView::operator-(const double& scalar) const {
    vector<double> result;
    result.reserve(length_);
    for (size_t i = 0; i < length_; ++i) result.push_back((*this)[i] - scalar);
    return materialise_("ViewChange " + getTimeSeriesId(), std::move(result));
}

TimeSeries TimeSeriesView::operator*(const double& scalar) const {
    vector<double> result;
    result.reserve(length_);
    for (size_t i = 0; i < length_; ++i) result.push_back((*this)[i] * scalar);
    return materialise_("ViewChange " + getTimeSeriesId(), std::move(result));
}

TimeSeries TimeSeriesView::operator+(const TimeSeriesView& other) const {
    ensure(isAlignedWith(other), "views are not aligned: {:s} vs {:s}", *this, other);
    vector<double> result;
    result.reserve(length_);
    for (size_t i = 0; i < length_; ++i) result.push_back((*this)[i] + other[i]);
    return materialise_("ViewChange " + getTimeSeriesId(), std::move(result));
}

TimeSeries TimeSeriesView::operator-(const TimeSeriesView& other) const {
    ensure(isAlignedWith(other), "views are not aligned: {:s} vs {:s}", *this, other);
    vector<double> result;
    result.reserve(length_);
    for (size_t i = 0; i < length_; ++i) result.push_back((*this)[i] - other[i]);
    return materialise_("ViewChange " + getTimeSeriesId(), std::move(result));
}

TimeSeries TimeSeriesView::operator*(const TimeSeriesView& other) const {
    ensure(isAlignedWith(other), "views are not aligned: {:s} vs {:s}", *this, other);
    vector<double> result;
    result.reserve(length_);
    for (size_t i = 0; i < length_; ++i) result.push_back((*this)[i] * other[i]);
    return materialise_(getTimeSeriesId() + " * " + other.getTimeSeriesId(), std::move(result));
}

bool TimeSeriesView::isAlignedWith(const TimeSeriesView& other) const {
    if (length_ != other.length_) return false;
    // Fast path: same physical timestamp range — pointer into the backing array is identical.
    if (source_->getTimestamps().data() + begin_ == other.source_->getTimestamps().data() + other.begin_) {
        return true;
    }
    // Slow path: element-by-element comparison.
    const auto ts1 = source_->getTimestamps();
    const auto ts2 = other.source_->getTimestamps();
    for (size_t i = 0; i < length_; ++i) {
        if (ts1[begin_ + i] != ts2[other.begin_ + i]) return false;
    }
    return true;
}

TimeSeries TimeSeriesView::toSeries() const {
    vector<double> result;
    result.reserve(length_);
    for (size_t i = 0; i < length_; i++) result.push_back((*this)[i]);
    return materialise_("View_Copy " + getTimeSeriesId(), std::move(result));
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

std::string TimeSeriesView::toString(const fmt::FormatSpec& spec) const {
    if (source_ == nullptr) return "TimeSeriesView [detached]";

    const bool empty = (length_ == 0 || source_->size() == 0);
    const auto values = empty ? std::span<const double>{} : std::span<const double>{begin(), length_};

    // A positive lag lets the value window run past the end of the timestamp vector, so
    // the dates are taken only as far as they actually exist. renderSeries drops the date
    // column when it gets a short span rather than reading off the end.
    std::span<const Timestamp> timestamps;
    if (!empty) {
        const auto sourceTimestamps = source_->getTimestamps();
        const size_t available =
            begin_ < sourceTimestamps.size() ? std::min(length_, sourceTimestamps.size() - begin_) : 0;
        timestamps = sourceTimestamps.subspan(begin_, available);
    }

    std::string identity = std::format("TimeSeriesView '{}'", source_->getId());
    if (empty) {
        identity += " [empty";
    } else {
        identity += std::format(" [n={}, rows {}..{}", length_, begin_, begin_ + length_ - 1);
        if (timestamps.size() == length_) {
            identity += std::format(", {} .. {}", fmt::AsDate{timestamps.front()}, fmt::AsDate{timestamps.back()});
        }
    }
    if (valueLag_ != 0) identity += std::format(", lag={}", valueLag_);
    identity += ']';

    switch (spec.mode) {
        case fmt::FormatMode::Identity:
            return identity;
        case fmt::FormatMode::Describe:
            return fmt::renderDescribe(identity, values, spec.precision);
        default:
            return fmt::renderSeries(identity, timestamps, values, spec);
    }
}

void TimeSeriesView::println(const fmt::FormatSpec& spec) const { std::println("{}", toString(spec)); }

void TimeSeriesView::head(size_t rows) const { println({.mode = fmt::FormatMode::Head, .count = rows}); }

void TimeSeriesView::tail(size_t rows) const { println({.mode = fmt::FormatMode::Tail, .count = rows}); }

void TimeSeriesView::describe() const { println({.mode = fmt::FormatMode::Describe}); }

RegularityCheck TimeSeriesView::checkRegularity(double tolerance) const {
    if (!cachedRegularityCheck_) {
        if (length_ < 3) {
            cachedRegularityCheck_ = (RegularityCheck{0.0, true, 0, 0.0});
        } else {
            std::vector<Timestamp> deltaT;
            deltaT.reserve(length_ - 1);
            for (size_t i = 1; i < length_; ++i) deltaT.push_back(timestamp(i) - timestamp(i - 1));
            auto deltaTCopy = deltaT;
            std::nth_element(deltaTCopy.begin(), deltaTCopy.begin() + deltaTCopy.size() / 2, deltaTCopy.end());
            Timestamp median = deltaTCopy[deltaTCopy.size() / 2];

            double mean = std::accumulate(deltaT.begin(), deltaT.end(), 0.0) / deltaT.size();
            double variance = 0.0;
            for (auto d : deltaT) variance += (d - mean) * (d - mean);
            variance /= deltaT.size();
            double standardDeviation = std::sqrt(variance);

            bool regular = (standardDeviation <= std::max(tolerance * static_cast<double>(median), 1.0));
            cachedRegularityCheck_ = RegularityCheck{tolerance, regular, median, standardDeviation};
        }
    } else {
        bool regular = (cachedRegularityCheck_->standardDeviationDeltaT <=
                        std::max(tolerance * static_cast<double>(cachedRegularityCheck_->medianDeltaT), 1.0));
        if (tolerance < cachedRegularityCheck_->cachedTolerance) {
            if (regular) {
                cachedRegularityCheck_->isRegular = regular;
                cachedRegularityCheck_->cachedTolerance = tolerance;
            } else {
                return RegularityCheck{tolerance,
                                       regular,
                                       cachedRegularityCheck_->medianDeltaT,
                                       cachedRegularityCheck_->standardDeviationDeltaT};
            }
        } else {
            if (cachedRegularityCheck_->isRegular) {
                return RegularityCheck{tolerance,
                                       true,
                                       cachedRegularityCheck_->medianDeltaT,
                                       cachedRegularityCheck_->standardDeviationDeltaT};
            } else {
                cachedRegularityCheck_->cachedTolerance = tolerance;
                cachedRegularityCheck_->isRegular = regular;
            }
        }
    }
    return cachedRegularityCheck_.value();
}
}  // namespace ts
