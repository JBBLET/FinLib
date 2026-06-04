// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <memory>
#include <utility>

#include "finapp/finance/asset/IAsset.hpp"
#include "finlib/analysis/TimeSeriesAnalysis.hpp"
namespace finance::analysis {

class IAssetAnalysis {
    IAssetAnalysis(std::shared_ptr<IAsset> asset, int64_t startMs, int64_t endMs, int64_t frequencyMs)
        : asset_{std::move(asset)},      //
          startMs_{startMs},             //
          endMs_{endMs},                 //
          frequencyMs_{frequencyMs} {};  //
    ~IAssetAnalysis() = default;

    virtual ::analysis::TimeSeriesAnalysis& priceAnalysis();
    virtual ::analysis::TimeSeriesAnalysis& returnAnalysis() = 0;
    std::shared_ptr<IAsset> getAsset() const { return asset_; }

 protected:
    int64_t startMs_;
    int64_t endMs_;
    int64_t frequencyMs_;
    std::shared_ptr<IAsset> asset_;

    ::analysis::TimeSeriesAnalysis priceAnalysis_;
    ::analysis::TimeSeriesAnalysis returnAnalysis_;
};
}  // namespace finance::analysis
