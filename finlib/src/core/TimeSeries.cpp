// Copyright 2026 JBBLET
#include "finlib/core/TimeSeries.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <future>
#include <optional>
#include <random>
#include <thread>
#include <utility>
#include <vector>

#include "finlib/core/TimeSeriesView.hpp"

using std::future;
using std::invalid_argument;
using std::sqrt;
using std::vector;

double calculateNoise(int64_t target, int64_t t1, int64_t t2, double annualVolatility, std::mt19937& gen) {
    std::normal_distribution<double> dist(0.0, 1.0);

    // scale the volatility with the distance to a known point
    double bridgeVarianceSeconds = (static_cast<double>(target - t1) * (t2 - target)) / (t2 - t1);

    // TODO PRECOMPUTRE THE annual_vol/sqrt(31560000.0) this is the same for all
    // the points
    double volatilityScale = annualVolatility * sqrt(bridgeVarianceSeconds / 31536000.0);
    return dist(gen) * volatilityScale;
}

double applyStrategy(InterpolationStrategy strategy, std::mt19937& gen, int64_t target, int64_t t1, double v1,
                     int64_t t2, double v2, double annualVolatility = 1.0) {
    double fraction = static_cast<double>(target - t1) / (t2 - t1);
    double linearVal = v1 + fraction * (v2 - v1);

    if (strategy == InterpolationStrategy::Linear) {
        return linearVal;
    }

    if (strategy == InterpolationStrategy::Stochastic) {
        return linearVal + calculateNoise(target, t1, t2, annualVolatility, gen);
    }
    if (strategy == InterpolationStrategy::Nearest) {
        return (target - t1 < t2 - target) ? t1 : t2;
    }
    return v1;
}

vector<double> TimeSeries::partialWalk(const vector<int64_t>& targetTimestamps, size_t startIndex, size_t endIndex,
                                       InterpolationStrategy strategy, std::optional<uint32_t> seed) const {
    static thread_local std::mt19937 globalGen(std::random_device{}());
    std::mt19937 localGen;
    if (seed) localGen.seed(*seed + startIndex);
    std::mt19937& currentGen = seed ? localGen : globalGen;

    const size_t chunkLength = endIndex - startIndex;
    vector<double> newValues;
    newValues.resize(chunkLength);

    auto it = std::lower_bound(timestamps_->begin(), timestamps_->end(), targetTimestamps[startIndex]);
    size_t dataIndex = std::distance(timestamps_->begin(), it);
    if (dataIndex > 0) dataIndex--;
    const size_t originalSize = timestamps_->size();

    for (size_t i = 0; i < chunkLength; i++) {
        int64_t currentTarget = targetTimestamps[startIndex + i];
        while (dataIndex < (originalSize - 1) && (*timestamps_)[dataIndex + 1] <= currentTarget) {
            dataIndex++;
        }
        if (currentTarget <= (*timestamps_)[0]) {
            newValues[i] = values_[0];
        } else if (dataIndex >= originalSize - 1) {
            newValues[i] = values_.back();
        } else {
            newValues[i] = applyStrategy(strategy, currentGen, currentTarget, (*timestamps_)[dataIndex],
                                         values_[dataIndex], (*timestamps_)[dataIndex + 1], values_[dataIndex + 1]);
        }
    }
    return newValues;
}

TimeSeries TimeSeries::resampling(const vector<int64_t>& targetTimestamps, InterpolationStrategy strategy,
                                  std::optional<uint32_t> seed) const {
    const size_t PARALLEL_THRESHOLD = 20000;
    if (!std::is_sorted(targetTimestamps.begin(), targetTimestamps.end())) {
        throw invalid_argument("target_timestamps must be sorted for resampling.");
    }

    if (targetTimestamps.size() < PARALLEL_THRESHOLD) {
        vector<double> newValues = partialWalk(targetTimestamps, 0, targetTimestamps.size(), strategy, seed);
        return TimeSeries("Resampled " + id_, targetTimestamps, std::move(newValues));
    } else {
        unsigned int numCores = std::thread::hardware_concurrency();
        size_t chunkSize = targetTimestamps.size() / numCores;

        vector<future<vector<double>>> futures;

        for (unsigned int i = 0; i < numCores; ++i) {
            size_t start = i * chunkSize;
            size_t end = (i == numCores - 1) ? targetTimestamps.size() : (i + 1) * chunkSize;

            futures.push_back(std::async(std::launch::async, [this, &targetTimestamps, start, end, strategy, seed]() {
                return this->partialWalk(targetTimestamps, start, end, strategy, seed);
            }));
        }
        vector<double> newValues;
        newValues.reserve(targetTimestamps.size());

        for (auto& fut : futures) {
            auto partial = fut.get();
            newValues.insert(newValues.end(), partial.begin(), partial.end());
        }
        return TimeSeries("Resampled " + id_, targetTimestamps, std::move(newValues));
    }
}

void TimeSeries::verifyAlignment(const TimeSeries& other) const {
    if (this->timestamps_ == other.timestamps_) {
        return;
    }
    if (this->size() != other.size()) {
        throw std::invalid_argument("TimeSeries size mismatch.");
    }
    if (!std::equal(timestamps_->begin(), timestamps_->end(), other.timestamps_->begin())) {
        throw std::invalid_argument("TimeSeries timestamps do not match.");
    }
}

TimeSeries TimeSeries::operator*(const TimeSeries& other) const {
    verifyAlignment(other);
    vector<double> resultValues;
    resultValues.resize(values_.size());
    std::transform(values_.begin(), values_.end(), other.values_.begin(), resultValues.begin(),
                   std::multiplies<double>());

    return TimeSeries(id_ + " * " + other.id_, this->timestamps_, std::move(resultValues));
}

TimeSeries& TimeSeries::operator*=(const TimeSeries& other) {
    verifyAlignment(other);
    for (size_t i = 0; i < values_.size(); ++i) {
        values_[i] *= other.values_[i];
    }
    return *this;
}

TimeSeries TimeSeries::operator*(double scalar) const {
    TimeSeries result = *this;
    result *= scalar;
    return result;
}
TimeSeries& TimeSeries::operator*=(double scalar) {
    for (double& v : values_) {
        v *= scalar;
    }
    return *this;
}

TimeSeries TimeSeries::operator+(const TimeSeries& other) const {
    verifyAlignment(other);
    vector<double> resultValues;
    resultValues.resize(values_.size());
    std::transform(values_.begin(), values_.end(), other.values_.begin(), resultValues.begin(), std::plus<double>());
    return TimeSeries(id_ + " + " + other.id_, this->timestamps_, std::move(resultValues));
}

TimeSeries& TimeSeries::operator+=(const TimeSeries& other) {
    verifyAlignment(other);
    for (size_t i = 0; i < values_.size(); ++i) {
        values_[i] += other.values_[i];
    }
    return *this;
}

TimeSeries TimeSeries::operator+(double scalar) const {
    TimeSeries result = *this;
    result += scalar;
    return result;
}

TimeSeries& TimeSeries::operator+=(double scalar) {
    for (double& v : values_) {
        v += scalar;
    }
    return *this;
}

TimeSeriesView TimeSeries::view() const { return TimeSeriesView(shared_from_this(), 0, values_.size()); }

TimeSeriesView TimeSeries::slice(size_t start, size_t len) const {
    return TimeSeriesView(shared_from_this(), start, len);
}

TimeSeriesView TimeSeries::sliceIndex(size_t start, size_t end) const {
    return TimeSeriesView(shared_from_this(), start, end - start + 1);
}
