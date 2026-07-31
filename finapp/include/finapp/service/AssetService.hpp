// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>
#include <unordered_map>

#include "finapp/data/providers/interfaces/IAssetProviders.hpp"
#include "finapp/data/repository/interface/IAssetRepository.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finapp/finance/common/AssetId.hpp"
#include "finlib/analysis/session/TimeSeriesSession.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"

using ts::InterpolationStrategy;
using ts::TimeSeries;
using ts::Timestamp;
using ts::Timestamps;
using ts::TimestampsPtr;
using ts::analysis::TimeSeriesSession;

namespace finapp {

class AssetService {
 public:
    AssetService(std::shared_ptr<ts::TimeSeriesService> timeSeriesService,
                 std::unordered_map<finance::AssetType, std::shared_ptr<IAssetRepository>> IAssetRepositoryMap,
                 std::unordered_map<finance::AssetType, std::shared_ptr<IAssetProvider>> IAssetProvidersMap);
    void save(const std::shared_ptr<finance::IAsset>& asset);

    std::shared_ptr<const finance::IAsset> load(const finance::AssetId& assetId);

    TimeSeries loadTimeSeriesValue(const finance::AssetId& assetId, Timestamp startMs, Timestamp endMs,
                                   Timestamp frequencyMs,
                                   InterpolationStrategy strategy = InterpolationStrategy::Nearest);

    // Overload sharing a caller-owned timestamp grid. Cash positions return a constant
    // 1.0 series in their own denomination (FX conversion is the caller's job).
    TimeSeries loadTimeSeriesValue(const finance::AssetId& assetId, TimestampsPtr timestamps);

    // Native observation timestamps in [startMs, endMs] — no resampling. Cash/unpriced
    // assets return an empty grid. Used to assemble a portfolio's analysis grid.
    TimestampsPtr rawTicks(const finance::AssetId& assetId, Timestamp startMs, Timestamp endMs);

    // Laod a singular point at a specific point to compute overwiew may be more performant based on repository
    // implementation
    double loadValueAtTs(const finance::AssetId& assetId, const Timestamp& timestamp);

    std::shared_ptr<TimeSeriesSession> createSession(const finance::AssetId& id, Timestamp startMs, Timestamp endMs,
                                                     Timestamp freqMs);
    std::shared_ptr<TimeSeriesSession> createSession(const finance::AssetId& id, TimestampsPtr timestamps);

 private:
    std::shared_ptr<ts::TimeSeriesService> timeSeriesService_;
    std::unordered_map<finance::AssetType, std::shared_ptr<IAssetRepository>> IAssetRepositoryMap_;
    std::unordered_map<finance::AssetType, std::shared_ptr<IAssetProvider>> IAssetProvidersMap_;
    std::unordered_map<finance::AssetId, std::shared_ptr<const finance::IAsset>> cachedAssets_;
};

}  // namespace finapp
