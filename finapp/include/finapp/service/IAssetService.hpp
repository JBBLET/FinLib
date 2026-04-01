// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "finapp/data/providers/interfaces/IAssetProviders.hpp"
#include "finapp/data/repository/IAssetRepository.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"
namespace finance {

struct AssetId {
    AssetType type;
    std::string ticker;
    bool operator==(const AssetId&) const = default;
};

class AssetService {
 public:
    AssetService(std::shared_ptr<TimeSeriesService> timeSeriesService,
                 std::unordered_map<AssetType, std::shared_ptr<IAssetRepository>> IAssetRepositoryMap);
    // should it return a unique pointer to a IAsset (the base class) instead of a element of IAsset by value to not
    // recomputeAndCache_
    void save(const IAsset* asset);

    IAsset load(const AssetId& assetId);

    TimeSeries loadTimeSeriesValue(const AssetId& assetId, int64_t startMs, int64_t endMs, int64_t frequencyMs);

 private:
    std::shared_ptr<TimeSeriesService> timeSeriesService_;
    std::unordered_map<AssetType, std::shared_ptr<IAssetRepository>> IAssetRepositoryMap_;
    std::unordered_map<AssetType, std::shared_ptr<IAssetProvider>> IAssetProvidersMap_;
};

}  // namespace finance
