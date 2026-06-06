// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>
#include <string>
#include <utility>

#include "finapp/finance/asset/IAsset.hpp"
#include "finlib/analysis/TimeSeriesAnalysis.hpp"
#include "finlib/session/TimeSeriesSession.hpp"

namespace finance::analysis {

class IAssetAnalysis {
 public:
    IAssetAnalysis(std::shared_ptr<IAsset> asset, std::shared_ptr<::analysis::TimeSeriesSession> session)
        : asset_{std::move(asset)}, session_{std::move(session)} {}

    virtual ~IAssetAnalysis() = default;

    // Price analysis delegates to the session's source series
    const ::analysis::TimeSeriesAnalysis& priceAnalysis() { return session_->sourceAnalysis(); }

    // Named derived analysis example: log returns or simple returns
    const ::analysis::TimeSeriesAnalysis& derivedAnalysis(const std::string& name) {
        return session_->derivedAnalysis(name);
    }

    // Range / frequency control
    void setRange(int64_t newStartMs, int64_t newEndMs) { session_->setRange(newStartMs, newEndMs); }
    void setFrequency(int64_t newFrequencyMs) { session_->setFrequency(newFrequencyMs); }

    // Direct session access — allows callers to register additional named transforms
    ::analysis::TimeSeriesSession& session() { return *session_; }

    std::shared_ptr<IAsset> asset() const { return asset_; }

 protected:
    std::shared_ptr<IAsset> asset_;
    std::shared_ptr<::analysis::TimeSeriesSession> session_;
};

}  // namespace finance::analysis
