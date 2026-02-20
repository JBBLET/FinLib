#include "finlib/core/TimeSeries.hpp"

#include <algorithm>
#include <cmath>
#include <execution>
#include <functional>
#include <future>
#include <iostream>
#include <optional>
#include <random>
#include <thread>
#include <vector>
#include <utility>

using std::future;
using std::invalid_argument;
using std::move;
using std::sqrt;
using std::vector;

double calculate_noise(int64_t target, int64_t t1, int64_t t2,
                       double annual_vol, std::mt19937& gen) {
    std::normal_distribution<double> dist(0.0, 1.0);

    // scale the volatility with the distance to a known point
    double bridge_variance_seconds =
        (static_cast<double>(target - t1) * (t2 - target)) / (t2 - t1);

    // TODO PRECOMPUTRE THE annual_vol/sqrt(31560000.0) this is the same for all
    // the points
    double vol_scale = annual_vol * sqrt(bridge_variance_seconds / 31536000.0);
    return dist(gen) * vol_scale;
}

double apply_strategy(InterpolationStrategy strategy, std::mt19937& gen,
                      int64_t target, int64_t t1, double v1, int64_t t2,
                      double v2, double annual_vol = 1.0) {
    double fraction = static_cast<double>(target - t1) / (t2 - t1);
    double linear_val = v1 + fraction * (v2 - v1);

    if (strategy == InterpolationStrategy::Linear) {
        return linear_val;
    }

    if (strategy == InterpolationStrategy::Stochastic) {
        return linear_val + calculate_noise(target, t1, t2, annual_vol, gen);
    }
    if (strategy == InterpolationStrategy::Nearest) {
        return (target-t1 < t2-target) ? t1 : t2;
    }
    return linear_val;
}

vector<double> TimeSeries::partial_walk(
    const vector<int64_t>& target_timestamps, size_t start_idx, size_t end_idx,
    InterpolationStrategy strategy, std::optional<uint32_t> seed) const {
    static thread_local std::mt19937 global_gen(std::random_device {} ());
    std::mt19937 local_gen;
    if (seed) local_gen.seed(*seed + start_idx);
    std::mt19937& current_gen = seed ? local_gen : global_gen;

    const size_t chunk_len = end_idx - start_idx;
    vector<double> new_values;
    new_values.resize(chunk_len);

    auto it = std::lower_bound(timestamps->begin(), timestamps->end(),
                               target_timestamps[start_idx]);
    size_t data_idx = std::distance(timestamps->begin(), it);
    if (data_idx > 0) data_idx--;
    const size_t original_len = timestamps->size();

    for (size_t i = 0; i < chunk_len; i++) {
        int64_t current_target = target_timestamps[start_idx + i];
        while (data_idx < (original_len - 1) &&
               (*timestamps)[data_idx + 1] <= current_target) {
            data_idx++;
        }
        if (current_target <= (*timestamps)[0]) {
            new_values[i] = values[0];
        } else if (data_idx >= original_len - 1) {
            new_values[i] = values.back();
        } else {
            new_values[i] = apply_strategy(
                strategy, current_gen, current_target, (*timestamps)[data_idx],
                values[data_idx], (*timestamps)[data_idx + 1],
                values[data_idx + 1]);
        }
    }
    return new_values;
}

TimeSeries TimeSeries::resampling(const vector<int64_t>& target_timestamps,
                                  InterpolationStrategy strategy,
                                  std::optional<uint32_t> seed) const {
    const size_t PARALLEL_THRESHOLD = 20000;
    if (!std::is_sorted(target_timestamps.begin(), target_timestamps.end())) {
        throw invalid_argument(
            "target_timestamps must be sorted for resampling.");
    }

    if (target_timestamps.size() < PARALLEL_THRESHOLD) {
        vector<double> new_values = partial_walk(
            target_timestamps, 0, target_timestamps.size(), strategy, seed);
        return TimeSeries(target_timestamps, std::move(new_values));
    } else {
        unsigned int num_cores = std::thread::hardware_concurrency();
        size_t chunk_size = target_timestamps.size() / num_cores;

        vector<future<vector<double>>> futures;

        for (unsigned int i = 0; i < num_cores; ++i) {
            size_t start = i * chunk_size;
            size_t end = (i == num_cores - 1) ? target_timestamps.size()
                                              : (i + 1) * chunk_size;

            futures.push_back(std::async(
                std::launch::async,
                [this, &target_timestamps, start, end, strategy, seed]() {
                    return this->partial_walk(target_timestamps, start, end,
                                              strategy, seed);
                }));
        }
        vector<double> new_values;
        new_values.reserve(target_timestamps.size());

        for (auto& fut : futures) {
            auto partial = fut.get();
            new_values.insert(new_values.end(), partial.begin(), partial.end());
        }
        return TimeSeries(target_timestamps, std::move(new_values));
    }
}

void TimeSeries::verify_alignment(const TimeSeries& other) const {
    if (this->timestamps == other.timestamps) {
        return;
    }
    if (this->size() != other.size()) {
        throw std::invalid_argument("TimeSeries size mismatch.");
    }
    if (!std::equal(timestamps->begin(), timestamps->end(),
                    other.timestamps->begin())) {
        throw std::invalid_argument("TimeSeries timestamps do not match.");
    }
}

TimeSeries TimeSeries::operator*(const TimeSeries& other) const {
    verify_alignment(other);
    vector<double> result_values;
    result_values.resize(values.size());
    std::transform(values.begin(), values.end(), other.values.begin(),
                   result_values.begin(), std::multiplies<double>());

    return TimeSeries(this->timestamps, std::move(result_values));
}

TimeSeries& TimeSeries::operator*=(const TimeSeries& other) {
    verify_alignment(other);
    for (size_t i = 0; i < values.size(); ++i) {
        values[i] *= other.values[i];
    }
    return *this;
}

TimeSeries TimeSeries::operator*(double scalar) const {
    TimeSeries result = *this;
    result *= scalar;
    return result;
}
TimeSeries& TimeSeries::operator*=(double scalar) {
    for (double& v : values) {
        v *= scalar;
    }
    return *this;
}

TimeSeries TimeSeries::operator+(const TimeSeries& other) const {
    verify_alignment(other);
    vector<double> result_values;
    result_values.resize(values.size());
    std::transform(values.begin(), values.end(), other.values.begin(),
                   result_values.begin(), std::plus<double>());
    return TimeSeries(this->timestamps, std::move(result_values));
}

TimeSeries& TimeSeries::operator+=(const TimeSeries& other) {
    verify_alignment(other);
    for (size_t i = 0; i < values.size(); ++i) {
        values[i] += other.values[i];
    }
    return *this;
}

TimeSeries TimeSeries::operator+(double scalar) const {
    TimeSeries result = *this;
    result += scalar;
    return result;
}

TimeSeries& TimeSeries::operator+=(double scalar) {
    for (double& v : values) {
        v += scalar;
    }
    return *this;
}


