// Copyright 2026 JBBLET
#include "finapp/finance/stats/FinanceStats.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include "finlib/core/StatsCore.hpp"
#include "finlib/core/TimeSeriesView.hpp"

namespace finapp::stats {

double volatility(const TimeSeriesView& returnSeries, double annualizationFactor) {
    return analysis::stats::standardDeviation(returnSeries) * std::sqrt(annualizationFactor);
}

double sharpeRatio(const TimeSeriesView& returnSeries, double annualizationFactor) {
    const double sigma = volatility(returnSeries, annualizationFactor);
    if (sigma == 0.0) return 0.0;
    return analysis::stats::mean(returnSeries) * std::sqrt(annualizationFactor) / sigma;
}

double totalReturn(const TimeSeriesView& priceSeries) {
    if (priceSeries.size() < 2) return std::numeric_limits<double>::quiet_NaN();
    const double first = priceSeries[0];
    if (first == 0.0) return std::numeric_limits<double>::quiet_NaN();
    return (priceSeries[priceSeries.size() - 1] - first) / first;
}

std::vector<std::vector<double>> correlationMatrix(const std::vector<const TimeSeriesView*>& views) {
    const size_t m = views.size();
    std::vector<std::vector<double>> result(m, std::vector<double>(m, 0.0));
    if (m == 0) return result;

    std::vector<double> means(m);
    std::vector<double> stddevs(m);
    for (size_t i = 0; i < m; ++i) {
        means[i]   = analysis::stats::mean(*views[i]);
        stddevs[i] = analysis::stats::standardDeviation(*views[i]);
    }

    const size_t n = views[0]->size();
    for (size_t a = 0; a < m; ++a) {
        result[a][a] = 1.0;
        for (size_t b = a + 1; b < m; ++b) {
            double cov = 0.0;
            for (size_t i = 0; i < n; ++i)
                cov += ((*views[a])[i] - means[a]) * ((*views[b])[i] - means[b]);
            cov /= static_cast<double>(n - 1);
            const double corr = (stddevs[a] > 0.0 && stddevs[b] > 0.0)
                                    ? cov / (stddevs[a] * stddevs[b])
                                    : 0.0;
            result[a][b] = result[b][a] = corr;
        }
    }
    return result;
}

}  // namespace finapp::stats
