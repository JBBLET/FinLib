// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "finapp/data/providers/interfaces/IAssetProviders.hpp"
#include "finapp/data/repository/interface/IAssetRepository.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finapp/finance/common/AssetId.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"

namespace finapp {

class AssetService {
 public:
    AssetService(std::shared_ptr<TimeSeriesService> timeSeriesService,
                 std::unordered_map<finance::AssetType, std::shared_ptr<IAssetRepository>> IAssetRepositoryMap,
                 std::unordered_map<finance::AssetType, std::shared_ptr<IAssetProvider>> IAssetProvidersMap);
    void save(const std::shared_ptr<finance::IAsset>& asset);

    std::shared_ptr<const finance::IAsset> load(const finance::AssetId& assetId);

    TimeSeries loadTimeSeriesValue(const finance::AssetId& assetId, int64_t startMs, int64_t endMs, int64_t frequencyMs);

    // Overload sharing a caller-owned timestamp grid. Cash positions return a constant
    // 1.0 series in their own denomination (FX conversion is the caller's job).
    TimeSeries loadTimeSeriesValue(const finance::AssetId& assetId, TimestampPtr timestamps);

 private:
    std::shared_ptr<TimeSeriesService> timeSeriesService_;
    std::unordered_map<finance::AssetType, std::shared_ptr<IAssetRepository>> IAssetRepositoryMap_;
    std::unordered_map<finance::AssetType, std::shared_ptr<IAssetProvider>> IAssetProvidersMap_;
    std::unordered_map<finance::AssetId, std::shared_ptr<const finance::IAsset>> cachedAssets_;
};

}  // namespace finapp
