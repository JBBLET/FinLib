// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "finapp/finance/analysis/AnalysisFeature.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finlib/analysis/seriesAnalysis/TimeSeriesAnalysis.hpp"
#include "finlib/analysis/session/TimeSeriesSession.hpp"

using ts::analysis::TimeSeriesAnalysis;
using ts::analysis::TimeSeriesSession;

namespace finance::analysis {

class IAssetAnalysis {
 public:
    IAssetAnalysis(std::shared_ptr<const IAsset> asset, std::shared_ptr<TimeSeriesSession> session)
        : asset_{std::move(asset)}, session_{std::move(session)} {}

    virtual ~IAssetAnalysis() = default;

    // Price analysis delegates to the session's source series
    const TimeSeriesAnalysis& priceAnalysis() { return session_->sourceAnalysis(); }

    // Named derived analysis example: log returns or simple returns
    const TimeSeriesAnalysis& derivedAnalysis(const std::string& name) { return session_->derivedAnalysis(name); }

    // Derived series registered by installed features (e.g. "logReturn", "simpleReturn").
    const std::vector<std::string>& derivedSeriesNames() const { return derivedSeries_; }

    // Install a feature (derived series + metrics) on this analysis's session.
    // Lets clients extend the analysis with arbitrary metrics — including
    // multi-series and parameterized ones — after construction.
    void installFeature(const FeatureInstaller& installer);
    void installFeatures(const std::vector<FeatureInstaller>& installers);

    // Scalar (double) metrics retained on the current window. Non-finite or
    // failing metrics are omitted; callers treat absent names as N/A. Richer
    // result types (matrices, series) are reachable via metrics() + customAnalysis().
    virtual std::vector<std::pair<std::string, double>> scalarMetrics();

    // All retained metrics, type-tagged, for consumers that surface non-scalar results.
    const std::vector<RetainedMetric>& metrics() const { return metrics_; }

    // Range / frequency control
    void setRange(int64_t newStartMs, int64_t newEndMs) { session_->setRange(newStartMs, newEndMs); }
    void setFrequency(int64_t newFrequencyMs) { session_->setFrequency(newFrequencyMs); }

    // Direct session access — allows callers to register additional named transforms
    TimeSeriesSession& session() { return *session_; }
    std::shared_ptr<TimeSeriesSession> sessionPtr() const { return session_; }

    std::shared_ptr<const IAsset> asset() const { return asset_; }

 protected:
    std::shared_ptr<const IAsset> asset_;
    std::shared_ptr<TimeSeriesSession> session_;

    // An asset's base series is its raw price — the session's primary series, named "".
    std::string base_;
    std::vector<std::string> derivedSeries_;
    std::vector<RetainedMetric> metrics_;
};

}  // namespace finance::analysis
