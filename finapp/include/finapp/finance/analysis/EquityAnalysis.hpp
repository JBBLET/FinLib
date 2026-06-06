// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <memory>
#include <utility>

#include "finapp/finance/analysis/IAssetAnalysis.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finlib/analysis/TimeSeriesAnalysis.hpp"
namespace finance::analysis {

class EquityAnalysis : public IAssetAnalysis {
    EquityAnalysis(std::shared_ptr<IAsset> asset, int64_t startMs, int64_t endMs, int64_t frequencyMs)
        : IAssetAnalysis{std::move(asset), startMs, endMs, frequencyMs} {};
    ~EquityAnalysis() = default;

    ::analysis::TimeSeriesAnalysis& priceAnalysis() override;
    ::analysis::TimeSeriesAnalysis& returnAnalysis() override;
};
}  // namespace finance::analysis
