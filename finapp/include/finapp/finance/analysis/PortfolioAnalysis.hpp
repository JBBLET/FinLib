// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "finapp/finance/analysis/IAssetAnalysis.hpp"
#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finlib/session/MultiTimeSeriesSession.hpp"

namespace finance::analysis {

// How the NAV cross-transform is computed.
//
//   TargetWeighted — weights are fractions summing to 1 (from targetAllocations).
//                    NAV(t) = sum(w_i * price_i(t) / price_i(t₀))
//                    Gives a normalized return index starting at ~1.0.
//
//   QuantityBased  — weights are share counts (from snapshot positions).
//                    NAV(t) = sum(q_i * price_i(t))
//                    Gives absolute portfolio value but is only correct at the snapshot
//                    time; use setNavTimeSeries() to override with transaction-replay data.
//
// For full transaction-based NAV, call setNavTimeSeries() with a pre-computed TimeSeries
// produced by PortfolioService (which can replay every trade).
enum class NavMode { TargetWeighted, QuantityBased };

class PortfolioAnalysis {
 public:
    PortfolioAnalysis(const finance::Portfolio& portfolio, std::unique_ptr<::analysis::MultiTimeSeriesSession> session,
                      std::vector<std::shared_ptr<IAssetAnalysis>> assetAnalyses,
                      std::unordered_map<std::string, double> navWeights, NavMode navMode);

    // Propagates to all sub-sessions via the MultiTimeSeriesSession.
    void setRange(int64_t startMs, int64_t endMs);
    void setFrequency(int64_t freqMs);

    // Override the computed NAV with a pre-built series (e.g. from transaction replay).
    // Once set, navSeries() / navAnalysis() use this instead of the cross-transform.
    void setNavTimeSeries(std::shared_ptr<const TimeSeries> nav);

    // NAV series and statistics.
    TimeSeriesView navSeries();
    const ::analysis::TimeSeriesAnalysis& navAnalysis();

    // Correlation / covariance matrices over price series by default.
    // Pass transformName = "return" for log-return correlation.
    std::vector<std::vector<double>> correlationMatrix(const std::string& transformName = "");
    std::vector<std::vector<double>> covarianceMatrix(const std::string& transformName = "");

    // Per-asset access
    std::shared_ptr<IAssetAnalysis> assetAnalysis(const std::string& ticker) const;
    const std::vector<std::shared_ptr<IAssetAnalysis>>& assetAnalyses() const;

    // Tickers in portfolio order (same order as assetAnalyses())
    const std::vector<std::string>& tickers() const;

    NavMode navMode() const { return navMode_; }

 private:
    std::unique_ptr<::analysis::MultiTimeSeriesSession> session_;
    std::vector<std::shared_ptr<IAssetAnalysis>> assetAnalyses_;
    std::unordered_map<std::string, std::shared_ptr<IAssetAnalysis>> index_;
    std::vector<std::string> tickers_;
    NavMode navMode_;

    // Set by setNavTimeSeries() — takes precedence over the cross-transform.
    std::unique_ptr<::analysis::TimeSeriesSession> navSession_;
    std::optional<::analysis::TimeSeriesAnalysis> precomputedNavAnalysis_;
};

}  // namespace finance::analysis
