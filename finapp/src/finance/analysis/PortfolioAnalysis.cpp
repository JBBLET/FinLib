// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/finance/analysis/PortfolioAnalysis.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finapp/finance/analysis/FinanceMetrics.hpp"
#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finlib/analysis/TimeSeriesAnalysis.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/session/TimeSeriesSession.hpp"

namespace finance::analysis {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
PortfolioAnalysis::PortfolioAnalysis(const finance::Portfolio& portfolio,
                                     std::unique_ptr<::analysis::MultiTimeSeriesSession> session,
                                     std::vector<std::shared_ptr<IAssetAnalysis>> assetAnalyses,
                                     std::unordered_map<std::string, double> navWeights, NavMode navMode)
    : session_{std::move(session)}, assetAnalyses_{std::move(assetAnalyses)}, navMode_{navMode} {
    tickers_.reserve(assetAnalyses_.size());
    for (const auto& a : assetAnalyses_) {
        const auto ticker = a->asset()->ticker();
        tickers_.push_back(ticker);
        index_[ticker] = a;
    }

    if (navMode_ == NavMode::TargetWeighted) {
        // NAV(t) = sum(w_i * price_i(t) / price_i(t₀))
        // Weights are fractions summing to 1; result is a return index starting at ~1.0.
        session_->addTransform("nav",
                               [w = std::move(navWeights)](
                                   const std::unordered_map<std::string, std::shared_ptr<const TimeSeries>> aligned) {
                                   if (aligned.empty())
                                       return TimeSeries("NAV", std::vector<int64_t>{}, std::vector<double>{});
                                   const auto& ref = *aligned.begin()->second;
                                   const size_t n = ref.size();
                                   std::vector<double> nav(n, 0.0);
                                   for (const auto& [ticker, series] : aligned) {
                                       auto wit = w.find(ticker);
                                       if (wit == w.end()) continue;
                                       const auto& vals = series->getValues();
                                       if (vals.empty()) continue;
                                       const double p0 = vals[0];
                                       for (size_t i = 0; i < n; ++i) nav[i] += wit->second * vals[i] / p0;
                                   }
                                   return TimeSeries("NAV", ref.getSharedTimestamps(), ref.tsOffset(), std::move(nav));
                               });
    } else {
        // NavMode::QuantityBased — NAV(t) = sum(q_i * price_i(t))
        // For proper historical accuracy, override via setNavTimeSeries().
        session_->addTransform("nav",
                               [q = std::move(navWeights)](
                                   const std::unordered_map<std::string, std::shared_ptr<const TimeSeries>> aligned) {
                                   if (aligned.empty())
                                       return TimeSeries("NAV", std::vector<int64_t>{}, std::vector<double>{});
                                   const auto& ref = *aligned.begin()->second;
                                   const size_t n = ref.size();
                                   std::vector<double> nav(n, 0.0);
                                   for (const auto& [ticker, series] : aligned) {
                                       auto qit = q.find(ticker);
                                       if (qit == q.end()) continue;
                                       const auto& vals = series->getValues();
                                       for (size_t i = 0; i < n; ++i) nav[i] += qit->second * vals[i];
                                   }
                                   return TimeSeries("NAV", ref.getSharedTimestamps(), ref.tsOffset(), std::move(nav));
                               });
    }

    navTotalReturnHandle_ =
        session_->customAnalysis("nav").addMetric("nav", "totalReturn", finapp::metrics::totalReturn());
}

// ---------------------------------------------------------------------------
// Range / frequency
// ---------------------------------------------------------------------------
void PortfolioAnalysis::setRange(int64_t startMs, int64_t endMs) {
    precomputedNavAnalysis_.reset();
    session_->setRange(startMs, endMs);
    if (navSession_) navSession_->setRange(startMs, endMs);
}

void PortfolioAnalysis::setFrequency(int64_t freqMs) {
    precomputedNavAnalysis_.reset();
    session_->setFrequency(freqMs);
    if (navSession_) navSession_->setFrequency(freqMs);
}

// ---------------------------------------------------------------------------
// Precomputed NAV override
// ---------------------------------------------------------------------------
void PortfolioAnalysis::setNavTimeSeries(std::shared_ptr<const TimeSeries> nav) {
    navSession_ = std::make_unique<::analysis::TimeSeriesSession>(std::move(nav));
    precomputedNavAnalysis_.reset();
}

// ---------------------------------------------------------------------------
// Portfolio-level analytics
// ---------------------------------------------------------------------------
TimeSeriesView PortfolioAnalysis::navSeries() {
    if (navSession_) return navSession_->sourceView();
    return session_->seriesView("nav");
}

const ::analysis::TimeSeriesAnalysis& PortfolioAnalysis::navAnalysis() {
    if (navSession_) {
        if (!precomputedNavAnalysis_.has_value()) precomputedNavAnalysis_ = ::analysis::TimeSeriesAnalysis(navSeries());
        return precomputedNavAnalysis_.value();
    }
    return session_->seriesAnalysis("nav");
}

const ::analysis::TimeSeriesAnalysis& PortfolioAnalysis::returnAnalysis() {
    if (navSession_) {
        if (!precomputedNavAnalysis_.has_value()) precomputedNavAnalysis_ = ::analysis::TimeSeriesAnalysis(navSeries());
        return precomputedNavAnalysis_.value();
    }
    return session_->seriesAnalysis("nav");
}

::analysis::CustomTimeSeriesAnalysis& PortfolioAnalysis::navCustomAnalysis() {
    if (navSession_) return navSession_->customAnalysis("");
    return session_->customAnalysis("nav");
}

// ---------------------------------------------------------------------------
// Per-asset access
// ---------------------------------------------------------------------------
std::shared_ptr<IAssetAnalysis> PortfolioAnalysis::assetAnalysis(const std::string& ticker) const {
    auto it = index_.find(ticker);
    if (it == index_.end()) throw std::runtime_error("No analysis for ticker: " + ticker);
    return it->second;
}

const std::vector<std::shared_ptr<IAssetAnalysis>>& PortfolioAnalysis::assetAnalyses() const { return assetAnalyses_; }

const std::vector<std::string>& PortfolioAnalysis::tickers() const { return tickers_; }

}  // namespace finance::analysis
