// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>
#include <utility>

#include "finapp/finance/asset/IAsset.hpp"
#include "finlib/analysis/TimeSeriesAnalysis.hpp"
namespace finance::analysis {

class IAssetAnalysis {
 public:
    IAssetAnalysis(std::shared_ptr<IAsset> asset, ::analysis::TimeSeriesAnalysis priceAnalysis,
                   ::analysis::TimeSeriesAnalysis returnAnalysis)
        : asset_{std::move(asset)}, priceAnalysis_{priceAnalysis}, returnAnalysis_{returnAnalysis} {}
    virtual ~IAssetAnalysis() = 0;

    virtual ::analysis::TimeSeriesAnalysis& priceAnalysis() = 0;
    virtual ::analysis::TimeSeriesAnalysis& returnAnalysis() = 0;
    std::shared_ptr<IAsset> getAsset() const { return asset_; }

 protected:
    std::shared_ptr<IAsset> asset_;

    ::analysis::TimeSeriesAnalysis priceAnalysis_;
    ::analysis::TimeSeriesAnalysis returnAnalysis_;
};
}  // namespace finance::analysis
