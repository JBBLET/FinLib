// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "finapp/data/providers/interfaces/IAssetProviders.hpp"
#include "finapp/data/repository/IAssetRepository.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finapp/finance/common/AssetId.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"

namespace finance {

class AssetService {
 public:
    AssetService(std::shared_ptr<TimeSeriesService> timeSeriesService,
                 std::unordered_map<AssetType, std::shared_ptr<IAssetRepository>> IAssetRepositoryMap);
    void save(const IAsset* asset);

    std::shared_ptr<const IAsset> load(const AssetId& assetId);

    TimeSeries loadTimeSeriesValue(const AssetId& assetId, int64_t startMs, int64_t endMs, int64_t frequencyMs);

 private:
    std::shared_ptr<TimeSeriesService> timeSeriesService_;
    std::unordered_map<AssetType, std::shared_ptr<IAssetRepository>> IAssetRepositoryMap_;
    std::unordered_map<AssetType, std::shared_ptr<IAssetProvider>> IAssetProvidersMap_;
    std::unordered_map<AssetId, std::shared_ptr<const IAsset>> cachedAssets_;
};

}  // namespace finance
