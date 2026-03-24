// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <Eigen/Dense>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "finlib/core/TimeSeries.hpp"

class TimeSeries;

struct RegularityCheck {
    double cachedTolerance;
    bool isRegular;
    int64_t medianDeltaT;
    double standardDeviationDeltaT;
    RegularityCheck(double tolerance, bool r, int64_t m, double sd)
        : cachedTolerance(tolerance), isRegular(r), medianDeltaT(m), standardDeviationDeltaT(sd) {}
};

class TimeSeriesView : public std::enable_shared_from_this<TimeSeriesView> {
 private:
    std::shared_ptr<const TimeSeries> source_;
    size_t begin_;
    size_t length_;
    int valueLag_;

    bool isAlignedWith(const TimeSeriesView& other) const;
    std::shared_ptr<std::vector<int64_t>> getCopyTimestampsInView() const;

    mutable std::optional<RegularityCheck> cachedRegularityCheck_;

 public:
    TimeSeriesView(std::shared_ptr<const TimeSeries> src, size_t start, size_t len, int lag = 0);
    TimeSeriesView() : source_(nullptr), begin_(0), length_(0), valueLag_(0) {}
    size_t size() const noexcept { return length_; }
    std::shared_ptr<const TimeSeriesView> getShared() const { return shared_from_this(); }
    std::string getTimeSeriesId() const { return source_->getId(); }

    const double* begin() const noexcept;
    const double* end() const noexcept;
    double operator[](size_t i) const;
    int64_t timestamp(size_t i) const;

    // methods modifying the range
    TimeSeriesView slice(size_t subStart, size_t subLength) const {
        return TimeSeriesView(source_, begin_ + subStart, subLength, valueLag_);
    }
    TimeSeriesView sliceIndex(size_t subStart, size_t subEnd) const {
        return TimeSeriesView(source_, begin_ + subStart, subEnd - subStart + 1, valueLag_);
    }

    // methods modifying the lag
    TimeSeriesView shift(int periods) const { return TimeSeriesView(source_, begin_, length_, valueLag_ + periods); }

    // Operation
    TimeSeries operator+(const double& scalar) const;
    TimeSeries operator-(const double& scalar) const;
    TimeSeries operator*(const double& scalar) const;

    TimeSeries operator+(const TimeSeriesView& other) const;
    TimeSeries operator-(const TimeSeriesView& other) const;
    TimeSeries operator*(const TimeSeriesView& other) const;

    // Transformation
    TimeSeries toSeries() const;

    inline Eigen::Map<const Eigen::VectorXd> asEigenVector() const {
        return Eigen::Map<const Eigen::VectorXd>(this->begin(), length_);
    }

    // Check
    RegularityCheck checkRegularity(double tolerance) const;
};
