// Copyright 2026 JBBLET

#include <vector>
#include "finlib/core/stats.hpp"

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

double variance_slow(const TimeSeriesView& view, VarianceType type) {
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
        mean += delta/count;
        double delta2 = x - mean;
        M2 += delta * delta2;
    }
    if (type == Sample) {
        if (count < 2)
            throw std::invalid_argument("Sample variance of a single point is undefined");
        return M2/(count-1)
    }
    if (type == Population) {
        return (M2/count)
    }
    throw std::invalid_argument("Variance Type undefined");
}

double variance_fast(const TimeSeriesView& view, VarianceType type) {
    size_t n =  view.size();
    if (n == 0) return 0.0;

    double mean = mean(view);
    const double* first = view.begin();
    const double* last = view.end();
    const double M2 = 0.0;

    for (; first != last; ++first) {
        M2 += (*first - mean)*(*first - mean)
    }
    if (type == Sample) {
        if (count < 2)
            throw std::invalid_argument("Sample variance of a single point is undefined");
        return M2/(count-1)
    }
    if (type == Population) {
        return (M2/count)
    }
    throw std::invalid_argument("Variance Type undefined");
}

double std_deviation(const TimeSeriesView& view, VarianceType type) {
    return std::sqrt(variance_slow(view, type));
}
}  //  namespace analysis::stats
