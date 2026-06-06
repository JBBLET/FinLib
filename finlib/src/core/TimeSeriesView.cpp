// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finlib/core/TimeSeriesView.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "finlib/core/TimeSeries.hpp"

using std::vector;

TimeSeriesView::TimeSeriesView(std::shared_ptr<const TimeSeries> src, size_t start, size_t len, int lag)
    : source_(std::move(src)), begin_(start), length_(len), valueLag_(lag) {
    if (static_cast<int>(begin_) - valueLag_ < 0 || (begin_ + length_ - valueLag_) > source_->size()) {
        throw std::out_of_range("View window/lag exceeds data boundaries");
    }
}

int64_t TimeSeriesView::timestamp(size_t i) const { return source_->getTimestamps()[begin_ + i]; }

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
    if (!isAlignedWith(other)) throw std::runtime_error("TimeSeries not aligned");
    vector<double> result;
    result.reserve(length_);
    for (size_t i = 0; i < length_; ++i) result.push_back((*this)[i] + other[i]);
    return materialise_("ViewChange " + getTimeSeriesId(), std::move(result));
}

TimeSeries TimeSeriesView::operator-(const TimeSeriesView& other) const {
    if (!isAlignedWith(other)) throw std::runtime_error("TimeSeries not aligned");
    vector<double> result;
    result.reserve(length_);
    for (size_t i = 0; i < length_; ++i) result.push_back((*this)[i] - other[i]);
    return materialise_("ViewChange " + getTimeSeriesId(), std::move(result));
}

TimeSeries TimeSeriesView::operator*(const TimeSeriesView& other) const {
    if (!isAlignedWith(other)) throw std::runtime_error("TimeSeries not aligned");
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

RegularityCheck TimeSeriesView::checkRegularity(double tolerance) const {
    if (!cachedRegularityCheck_) {
        if (length_ < 3) {
            cachedRegularityCheck_ = (RegularityCheck{0.0, true, 0, 0.0});
        } else {
            std::vector<int64_t> deltaT;
            deltaT.reserve(length_ - 1);
            for (size_t i = 1; i < length_; ++i) deltaT.push_back(timestamp(i) - timestamp(i - 1));
            auto deltaTCopy = deltaT;
            std::nth_element(deltaTCopy.begin(), deltaTCopy.begin() + deltaTCopy.size() / 2, deltaTCopy.end());
            int64_t median = deltaTCopy[deltaTCopy.size() / 2];

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
