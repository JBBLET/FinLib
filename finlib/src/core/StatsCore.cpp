// Copyright 2026 JBBLET

#include "finlib/core/StatsCore.hpp"

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <vector>

#include "Eigen/Core"
#include "finlib/common/Error.hpp"
using std::size_t;

namespace ts::analysis::stats {

double mean(Samples x) {
    if (x.empty()) return 0.0;

    size_t count{0};
    double sum{0.0};
    for (double v : x) {
        if (std::isfinite(v)) {
            sum += v;
            ++count;
        }
    }
    if (count == 0) return 0.0;
    return sum / static_cast<double>(count);
}

double varianceFast(Samples x, VarianceType type) {
    // TODO(JBBLET) Look into the parallel algorithm to at least improve a bit;
    if (x.empty()) return 0.0;
    double M2{0.0}, mean{0.0};
    size_t count{0};
    for (double v : x) {
        if (std::isfinite(v)) {
            ++count;
            double delta = v - mean;
            mean += delta / static_cast<double>(count);
            double delta2 = v - mean;
            M2 += delta * delta2;
        }
    }
    if (count == 0) return 0.0;
    if (type == VarianceType::Sample) {
        ensure<InvalidArgument>(count >= 2, "Sample variance of a single point is undefined");
        return M2 / static_cast<double>(count - 1);
    }
    if (type == VarianceType::Population) {
        return M2 / static_cast<double>(count);
    }
    throw InvalidArgument("Variance Type undefined");
}

double varianceSlow(Samples x, VarianceType type) {
    if (x.empty()) return 0.0;

    double avg{mean(x)};
    double M2{0.0};
    size_t count{0};

    for (double v : x) {
        if (std::isfinite(v)) {
            M2 += (v - avg) * (v - avg);
            ++count;
        }
    }
    if (count == 0) return 0.0;
    if (type == VarianceType::Sample) {
        ensure<InvalidArgument>(count >= 2, "Sample variance of a single point is undefined");
        return M2 / static_cast<double>(count - 1);
    }
    if (type == VarianceType::Population) {
        return M2 / static_cast<double>(count);
    }
    throw InvalidArgument("Variance Type undefined");
}

double standardDeviation(Samples x, VarianceType type) { return std::sqrt(varianceFast(x, type)); }

double skewness(Samples x) {
    if (x.empty()) return 0.0;

    double avg = mean(x);
    double sd = standardDeviation(x, VarianceType::Population);
    if (sd == 0.0) return 0.0;  // degenerate sample: no shape to report

    double M3 = 0.0;
    size_t count{0};
    for (double v : x) {
        if (std::isfinite(v)) {
            double z = (v - avg) / sd;
            M3 += z * z * z;
            ++count;
        }
    }
    if (count == 0) return 0.0;
    return M3 / static_cast<double>(count);
}

namespace {
double standardizedFourthMoment(Samples x) {
    double avg = mean(x);
    double sd = standardDeviation(x, VarianceType::Population);
    if (sd == 0.0) return 0.0;

    double M4 = 0.0;
    size_t count{0};
    for (double v : x) {
        if (std::isfinite(v)) {
            double z2 = ((v - avg) / sd) * ((v - avg) / sd);
            M4 += z2 * z2;
            ++count;
        }
    }
    ensure<InvalidArgument>(count >= 4, "Kurtosis undefined");
    return M4 / static_cast<double>(count);
}
}  // namespace

double kurtosis(Samples x) {
    if (x.empty()) return 0.0;
    return standardizedFourthMoment(x);
}

double excessKurtosis(Samples x) {
    if (x.empty()) return 0.0;
    return standardizedFourthMoment(x) - 3.0;
}

double quantileSorted(Samples sortedX, double q) {
    ensure<InvalidArgument>(!sortedX.empty(), "quantileSorted: empty sample");
    ensure<InvalidArgument>(q >= 0.0 && q <= 1.0, "quantileSorted: q must lie in [0, 1]");

    const double position = q * static_cast<double>(sortedX.size() - 1);
    const auto lower = static_cast<size_t>(std::floor(position));
    const auto upper = static_cast<size_t>(std::ceil(position));
    if (lower == upper) return sortedX[lower];

    const double weight = position - static_cast<double>(lower);
    return sortedX[lower] * (1.0 - weight) + sortedX[upper] * weight;
}

double autocorrelationAt(Samples x, size_t lag) {
    const size_t n = x.size();
    if (n == 0 || lag >= n) return 0.0;

    double avg = mean(x);
    double den = static_cast<double>(n) * varianceFast(x, VarianceType::Population);
    if (den == 0.0) return 0.0;

    double num = 0.0;
    for (size_t i = lag; i < n; ++i) num += (x[i] - avg) * (x[i - lag] - avg);

    return num / den;
}

std::vector<double> acf(Samples x, size_t maxLag) {
    const size_t n = x.size();
    if (n == 0) return {};
    const size_t actualMaxLag = std::min(maxLag, n - 1);

    double avg = mean(x);

    double denominator = 0.0;
    for (double v : x) denominator += (v - avg) * (v - avg);

    if (denominator == 0.0) return std::vector<double>(actualMaxLag + 1, 0.0);

    std::vector<double> acfCoeffs;
    acfCoeffs.reserve(actualMaxLag + 1);
    for (size_t lag = 0; lag <= actualMaxLag; ++lag) {
        double numerator = 0.0;
        for (size_t i = 0; i < n - lag; ++i) numerator += (x[i] - avg) * (x[i + lag] - avg);
        acfCoeffs.push_back(numerator / denominator);
    }
    return acfCoeffs;
}

std::vector<double> autocovariances(Samples x, size_t maxLag) {
    const size_t n = x.size();
    if (n == 0) return {};
    const size_t actualMaxLag = std::min(maxLag, n - 1);

    double avg = mean(x);
    std::vector<double> gamma;
    gamma.reserve(actualMaxLag + 1);
    for (size_t lag = 0; lag <= actualMaxLag; ++lag) {
        double value = 0.0;
        for (size_t i = 0; i < n - lag; ++i) value += (x[i] - avg) * (x[i + lag] - avg);
        gamma.push_back(value);
    }
    return gamma;
}

Eigen::MatrixXd toeplitzFromSamples(Samples x, size_t maxLag) {
    return toeplitzFromAutocovariances(autocovariances(x, maxLag), std::min(maxLag, x.empty() ? 0 : x.size() - 1));
}

Eigen::MatrixXd toeplitzFromAutocovariances(Samples gamma, size_t maxLag) {
    ensure(gamma.size() >= maxLag, "autocovariances size {} is smaller than the toeplitz matrix requires ({})",
           gamma.size(), maxLag);
    Eigen::MatrixXd R(maxLag, maxLag);
    for (size_t i = 0; i < maxLag; ++i) {
        for (size_t j = 0; j < maxLag; ++j) {
            R(i, j) = gamma[i > j ? i - j : j - i];
        }
    }
    return R;
}
}  // namespace ts::analysis::stats

namespace ts::analysis::hypothesisTesting {

double PvalueFromTStatistic(double tStat) { return std::erfc(std::abs(tStat) / std::sqrt(2.0)); }

}  // namespace ts::analysis::hypothesisTesting
