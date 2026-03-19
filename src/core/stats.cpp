// Copyright 2026 JBBLET

#include "finlib/core/stats.hpp"

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <vector>

#include "Eigen/Core"
#include "finlib/core/TimeSeriesView.hpp"
using std::size_t;

namespace analysis::stats {

double mean(const TimeSeriesView& view) {
    if (view.size() == 0) return 0.0;

    double sum = 0.0;
    const double* first = view.begin();
    const double* last = view.end();

    for (; first != last; ++first) {
        sum += *first;
    }

    return sum / static_cast<double>(view.size());
}

double varianceFast(const TimeSeriesView& view, VarianceType type) {
    // TODO(JBBLET) Look into the parallel algorithm to at least improve a bit;
    size_t n = view.size();
    if (n == 0) return 0.0;

    double M2 = 0.0, mean = 0.0;
    const double* first = view.begin();
    const double* last = view.end();
    size_t count = 0;
    for (; first != last; ++first) {
        ++count;
        double x = *first;
        double delta = x - mean;
        mean += delta / count;
        double delta2 = x - mean;
        M2 += delta * delta2;
    }
    if (type == VarianceType::Sample) {
        if (count < 2) throw std::invalid_argument("Sample variance of a single point is undefined");
        return M2 / (count - 1);
    }
    if (type == VarianceType::Population) {
        return (M2 / count);
    }
    throw std::invalid_argument("Variance Type undefined");
}

double varianceSlow(const TimeSeriesView& view, VarianceType type) {
    size_t n = view.size();
    if (n == 0) return 0.0;

    double avg = mean(view);
    const double* first = view.begin();
    const double* last = view.end();
    double M2 = 0.0;

    for (; first != last; ++first) {
        M2 += (*first - avg) * (*first - avg);
    }
    if (type == VarianceType::Sample) {
        if (n < 2) throw std::invalid_argument("Sample variance of a single point is undefined");
        return M2 / (n - 1);
    }
    if (type == VarianceType::Population) {
        return (M2 / n);
    }
    throw std::invalid_argument("Variance Type undefined");
}

double standardDeviation(const TimeSeriesView& view, VarianceType type) {
    double variance = varianceFast(view, type);
    return std::sqrt(variance);
}

double skewness(const TimeSeriesView& view) {
    size_t n = view.size();
    if (n == 0) return 0.0;

    double avg = mean(view);
    double std = standardDeviation(view, VarianceType::Population);
    const double* first = view.begin();
    const double* last = view.end();
    double M3 = 0.0;
    for (; first != last; ++first) {
        double z = (*first - avg) / std;
        double z2 = z * z;
        M3 += z2 * z;
    }
    return M3 / n;
}

double kurtosis(const TimeSeriesView& view) {
    size_t n = view.size();
    if (n == 0) return 0.0;

    double avg = mean(view);
    double std = standardDeviation(view, VarianceType::Population);
    const double* first = view.begin();
    const double* last = view.end();
    double M3 = 0.0;
    for (; first != last; ++first) {
        double z = (*first - avg) / std;
        double z2 = z * z;
        M3 += z2 * z2;
    }
    if (n < 4) throw std::invalid_argument("Kurtosis undefined");
    return M3 / n;
}

double excessKurtosis(const TimeSeriesView& view) {
    size_t n = view.size();
    if (n == 0) return 0.0;

    double avg = mean(view);
    double std = standardDeviation(view, VarianceType::Population);
    const double* first = view.begin();
    const double* last = view.end();
    double M3 = 0.0;
    for (; first != last; ++first) {
        double z = (*first - avg) / std;
        double z2 = z * z;
        M3 += z2 * z2;
    }
    if (n < 4) throw std::invalid_argument("Kurtosis undefined");
    return M3 / n - 3;
}

double autocorrelationAt(const TimeSeriesView& view, size_t lag) {
    size_t n = view.size();
    const double* start = view.begin();
    double avg = mean(view);

    double den = n * varianceFast(view, VarianceType::Population);

    if (den == 0.0) return 0.0;

    double num = 0.0;
    const double* current = start + lag;
    const double* delayed = start;

    for (size_t i = lag; i < n; ++i) {
        num += (*current - avg) * (*delayed - avg);
        current++;
        delayed++;
    }

    return num / den;
}

std::vector<double> acf(const TimeSeriesView& view, size_t maxLag) {
    size_t n = view.size();
    size_t actualMaxLag = std::min(maxLag, n - 1);
    std::vector<double> acfCoeffs;
    acfCoeffs.reserve(actualMaxLag + 1);

    double avg = mean(view);

    double denominator = 0.0;
    const double* data = view.begin();
    for (size_t i = 0; i < n; ++i) {
        double d = data[i] - avg;
        denominator += d * d;
    }

    if (denominator == 0.0) {
        return std::vector<double>(actualMaxLag + 1, 0.0);
    }

    for (size_t lag = 0; lag <= actualMaxLag; ++lag) {
        double numerator = 0.0;
        const double* current = data + lag;
        const double* delayed = data;

        for (size_t i = 0; i < n - lag; ++i) {
            numerator += (*delayed - avg) * (*current - avg);
            delayed++;
            current++;
        }
        acfCoeffs.push_back(numerator / denominator);
    }
    return acfCoeffs;
}

std::vector<double> autocovariances(const TimeSeriesView& view, size_t maxLag) {
    size_t n = view.size();
    size_t actualMaxLag = std::min(maxLag, n - 1);
    std::vector<double> gamma;
    gamma.reserve(actualMaxLag + 1);
    double avg = mean(view);
    const double* data = view.begin();
    for (size_t lag = 0; lag <= actualMaxLag; ++lag) {
        double value = 0.0;
        const double* current = data + lag;
        const double* delayed = data;
        for (size_t i = 0; i < n - lag; ++i) {
            value += (*delayed - avg) * (*current - avg);
            delayed++;
            current++;
        }
        gamma.push_back(value);
    }
    return gamma;
}

Eigen::MatrixXd toeplitz(const TimeSeriesView& view, size_t maxLag) {
    size_t n = view.size();
    size_t actualMaxLag = std::min(maxLag, n - 1);
    Eigen::MatrixXd R(actualMaxLag, actualMaxLag);
    std::vector<double> gamma = autocovariances(view, maxLag);
    for (size_t i = 0; i < actualMaxLag; ++i) {
        for (size_t j = 0; j < actualMaxLag; ++j) {
            R(i, j) = gamma[std::abs(static_cast<int>(i - j))];
        }
    }
    return R;
}

Eigen::MatrixXd toeplitz(const std::vector<double>& gamma, size_t maxLag) {
    size_t n = gamma.size();
    if (n < maxLag - 1)
        throw std::runtime_error("autocovariances size do not match the required for the toeplitz matrix");
    Eigen::MatrixXd R(maxLag, maxLag);
    for (size_t i = 0; i < maxLag; ++i) {
        for (size_t j = 0; j < maxLag; ++j) {
            R(i, j) = gamma[std::abs(static_cast<int>(i - j))];
        }
    }
    return R;
}
}  //  namespace analysis::stats
