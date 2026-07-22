// Copyright 2026 JBBLET
#pragma once

#include <vector>

namespace ts {
class TimeSeriesView;
}  // namespace ts

using ts::TimeSeriesView;
namespace finapp::stats {

// Annualized volatility — assumes the view is already a return series.
// volatility = standardDeviation(returns) * sqrt(annualizationFactor)
double volatility(const TimeSeriesView& returnSeries, double annualizationFactor = 252.0);

// Annualized Sharpe ratio — assumes the view is already a return series.
// sharpe = mean(returns) / standardDeviation(returns) * sqrt(annualizationFactor)
double sharpeRatio(const TimeSeriesView& returnSeries, double annualizationFactor = 252.0);

// Total return from a price series: (last - first) / first
double totalReturn(const TimeSeriesView& priceSeries);

// Pearson correlation matrix over an ordered set of return series.
// Result is row-major: result[i][j] = correlation between views[i] and views[j].
std::vector<std::vector<double>> correlationMatrix(const std::vector<const TimeSeriesView*>& views);

}  // namespace finapp::stats
