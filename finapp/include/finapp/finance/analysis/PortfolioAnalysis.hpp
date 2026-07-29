// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finapp/finance/analysis/AnalysisFeature.hpp"
#include "finapp/finance/analysis/IAssetAnalysis.hpp"
#include "finapp/finance/portfolio/Portfolio.hpp"
#include "finlib/analysis/seriesAnalysis/CustomTimeSeriesAnalysis.hpp"
#include "finlib/analysis/seriesAnalysis/TimeSeriesAnalysis.hpp"
#include "finlib/analysis/session/ITimeSeriesSession.hpp"
#include "finlib/analysis/session/MultiTimeSeriesSession.hpp"
#include "finlib/analysis/session/TimeSeriesSession.hpp"
#include "finlib/core/TimeSeries.hpp"

using ts::TimeSeries;
using ts::TimeSeriesView;
using ts::analysis::CustomTimeSeriesAnalysis;
using ts::analysis::ITimeSeriesSession;
using ts::analysis::MultiTimeSeriesSession;
using ts::analysis::TimeSeriesAnalysis;
using ts::analysis::TimeSeriesSession;

namespace finance::analysis {

enum class NavMode { TargetWeighted, QuantityBased };

class PortfolioAnalysis {
 public:
    PortfolioAnalysis(const finance::Portfolio& portfolio, std::unique_ptr<MultiTimeSeriesSession> session,
                      std::vector<std::shared_ptr<IAssetAnalysis>> assetAnalyses,
                      std::unordered_map<std::string, double> navWeights, NavMode navMode);

    void setRange(int64_t startMs, int64_t endMs);
    void setFrequency(int64_t freqMs);

    void setNavTimeSeries(std::shared_ptr<const TimeSeries> nav);

    void installFeature(const FeatureInstaller& installer);

    std::vector<std::string> seriesNames() const;
    TimeSeriesView seriesView(const std::string& name);
    const TimeSeriesAnalysis& seriesAnalysis(const std::string& name);
    CustomTimeSeriesAnalysis& customAnalysis(const std::string& name);

    const std::vector<std::string>& derivedSeriesNames() const { return derivedSeries_; }

    std::vector<std::pair<std::string, double>> scalarMetrics();
    const std::vector<RetainedMetric>& metrics() const { return metrics_; }

    std::shared_ptr<IAssetAnalysis> assetAnalysis(const std::string& ticker) const;
    const std::vector<std::shared_ptr<IAssetAnalysis>>& assetAnalyses() const;

    const std::vector<std::string>& tickers() const;

    NavMode navMode() const { return navMode_; }

 private:
    std::unique_ptr<MultiTimeSeriesSession> session_;
    std::vector<std::shared_ptr<IAssetAnalysis>> assetAnalyses_;
    std::unordered_map<std::string, std::shared_ptr<IAssetAnalysis>> index_;
    std::vector<std::string> tickers_;
    NavMode navMode_;

    std::unique_ptr<TimeSeriesSession> navSession_;

    std::vector<FeatureInstaller> installers_;
    std::vector<std::string> derivedSeries_;
    std::vector<RetainedMetric> metrics_;

    ITimeSeriesSession* activeNavSession_ = nullptr;
    std::string activeBase_;

    std::string resolveName_(const std::string& name) const;
    void applyInstallers_();
};

}  // namespace finance::analysis
