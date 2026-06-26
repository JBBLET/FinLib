// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/finance/analysis/PortfolioAnalysis.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finapp/common/Exception.hpp"
#include "finapp/finance/analysis/AnalysisFeature.hpp"
#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/core/TimeSeriesView.hpp"
#include "finlib/session/TimeSeriesSession.hpp"

namespace finance::analysis {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
PortfolioAnalysis::PortfolioAnalysis(const finance::Portfolio& portfolio,
                                     std::unique_ptr<MultiTimeSeriesSession> session,
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
                                       const size_t useN = std::min(n, vals.size());
                                       for (size_t i = 0; i < useN; ++i) nav[i] += wit->second * vals[i] / p0;
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
                                       const size_t useN = std::min(n, vals.size());
                                       for (size_t i = 0; i < useN; ++i) nav[i] += qit->second * vals[i];
                                   }
                                   return TimeSeries("NAV", ref.getSharedTimestamps(), ref.tsOffset(), std::move(nav));
                               });
    }

    activeNavSession_ = session_.get();
    activeBase_ = "nav";
}

// ---------------------------------------------------------------------------
// Feature installation
// ---------------------------------------------------------------------------
void PortfolioAnalysis::applyInstallers_() {
    derivedSeries_.clear();
    metrics_.clear();
    for (const auto& installer : installers_) {
        FeatureBindings bindings = installer(*activeNavSession_, activeBase_);
        derivedSeries_.insert(derivedSeries_.end(), bindings.derivedSeries.begin(), bindings.derivedSeries.end());
        for (auto& m : bindings.metrics) metrics_.push_back(std::move(m));
    }
}

void PortfolioAnalysis::installFeature(const FeatureInstaller& installer) {
    installers_.push_back(installer);
    FeatureBindings bindings = installer(*activeNavSession_, activeBase_);
    derivedSeries_.insert(derivedSeries_.end(), bindings.derivedSeries.begin(), bindings.derivedSeries.end());
    for (auto& m : bindings.metrics) metrics_.push_back(std::move(m));
}

// ---------------------------------------------------------------------------
// Range / frequency
// ---------------------------------------------------------------------------
void PortfolioAnalysis::setRange(int64_t startMs, int64_t endMs) {
    session_->setRange(startMs, endMs);
    if (navSession_) navSession_->setRange(startMs, endMs);
}

void PortfolioAnalysis::setFrequency(int64_t freqMs) {
    session_->setFrequency(freqMs);
    if (navSession_) navSession_->setFrequency(freqMs);
}

// ---------------------------------------------------------------------------
// Precomputed NAV override
// ---------------------------------------------------------------------------
void PortfolioAnalysis::setNavTimeSeries(std::shared_ptr<const TimeSeries> nav) {
    navSession_ = std::make_unique<TimeSeriesSession>(std::move(nav));
    // The override's source IS the NAV — addressed as the primary series, "".
    activeNavSession_ = navSession_.get();
    activeBase_ = "";
    applyInstallers_();
}

// ---------------------------------------------------------------------------
// Uniform per-series access
// ---------------------------------------------------------------------------
std::string PortfolioAnalysis::resolveName_(const std::string& name) const {
    return name == "nav" ? activeBase_ : name;
}

std::vector<std::string> PortfolioAnalysis::seriesNames() const {
    std::vector<std::string> names;
    names.reserve(1 + derivedSeries_.size());
    names.push_back("nav");
    names.insert(names.end(), derivedSeries_.begin(), derivedSeries_.end());
    return names;
}

TimeSeriesView PortfolioAnalysis::seriesView(const std::string& name) {
    return activeNavSession_->seriesView(resolveName_(name));
}

const TimeSeriesAnalysis& PortfolioAnalysis::seriesAnalysis(const std::string& name) {
    return activeNavSession_->seriesAnalysis(resolveName_(name));
}

CustomTimeSeriesAnalysis& PortfolioAnalysis::customAnalysis(const std::string& name) {
    return activeNavSession_->customAnalysis(resolveName_(name));
}

std::vector<std::pair<std::string, double>> PortfolioAnalysis::scalarMetrics() {
    return computeMetricsOfType<double>(*activeNavSession_, metrics_);
}

// ---------------------------------------------------------------------------
// Per-asset access
// ---------------------------------------------------------------------------
std::shared_ptr<IAssetAnalysis> PortfolioAnalysis::assetAnalysis(const std::string& ticker) const {
    auto it = index_.find(ticker);
    if (it == index_.end()) throw finapp::Exception("No analysis for ticker: " + ticker);
    return it->second;
}

const std::vector<std::shared_ptr<IAssetAnalysis>>& PortfolioAnalysis::assetAnalyses() const { return assetAnalyses_; }

const std::vector<std::string>& PortfolioAnalysis::tickers() const { return tickers_; }

}  // namespace finance::analysis
