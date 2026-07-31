// Copyright (c) 2026 JBBLET. All Rights Reserved.

#include "finapp/service/AssetService.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "finapp/common/Error.hpp"
#include "finapp/common/Log.hpp"
#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/asset/Cash.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finlib/analysis/session/TimeSeriesSession.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/utils/TimeSeriesUtils.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/services/TimeSeriesService.hpp"

using ts::TimeSeriesService;
namespace finapp {

using finance::AssetId;
using finance::AssetType;
using finance::IAsset;

namespace {

// Cash assets have no price series — they are their own numeraire. Return a
// constant 1.0 series in the cash's denomination; FX conversion is the caller's job.
TimeSeries constantCashSeries(const AssetId& assetId, Timestamp startMs, Timestamp endMs, Timestamp frequencyMs) {
    return ts::common::utils::timeSeries::generateConstantTimeSeries(assetId.ticker, startMs, endMs, frequencyMs, 1.0);
}

TimeSeries constantCashSeries(const AssetId& assetId, TimestampsPtr timestamps) {
    return ts::common::utils::timeSeries::generateConstantTimeSeries(assetId.ticker, std::move(timestamps), 1.0);
}

}  // namespace

AssetService::AssetService(std::shared_ptr<TimeSeriesService> timeSeriesService,
                           std::unordered_map<AssetType, std::shared_ptr<IAssetRepository>> IAssetRepositoryMap,
                           std::unordered_map<AssetType, std::shared_ptr<IAssetProvider>> IAssetProvidersMap)
    : timeSeriesService_(std::move(timeSeriesService)),
      IAssetRepositoryMap_(std::move(IAssetRepositoryMap)),
      IAssetProvidersMap_(std::move(IAssetProvidersMap)) {}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void AssetService::save(const std::shared_ptr<IAsset>& asset) {
    ensure<InvalidArgument>(asset != nullptr, "AssetService::save: asset pointer is null.");

    auto repoIt = IAssetRepositoryMap_.find(asset->type());
    ensure(repoIt != IAssetRepositoryMap_.end(), "AssetService::save: no repository registered for asset type {}.",
           assetTypeToString(asset->type()));

    repoIt->second->save(asset);
    cachedAssets_[AssetId{asset->type(), asset->ticker()}] = asset;
}

std::shared_ptr<const finance::IAsset> AssetService::load(const AssetId& assetId) {
    // 1. In-memory cache hit.
    if (auto it = cachedAssets_.find(assetId); it != cachedAssets_.end()) {
        return it->second;
    }

    // Cash is synthesized on demand — it has no repository row and no provider.
    if (assetId.type == AssetType::Cash) {
        auto cash = std::make_shared<const finance::Cash>(finance::currencyFromString(assetId.ticker.substr(0, 3)));
        cachedAssets_[assetId] = cash;
        return cash;
    }

    // 2. Repository hit.
    auto repoIt = IAssetRepositoryMap_.find(assetId.type);
    if (repoIt != IAssetRepositoryMap_.end() && repoIt->second->exists(assetId.ticker)) {
        auto asset = repoIt->second->load(assetId.ticker);
        if (asset) {
            logging::debug("load: '{}' from repository", assetId.ticker);
            cachedAssets_[assetId] = asset;
            return asset;
        }
    }

    // 3. Provider fallback — persist back into the repository so next call hits step 2.
    auto providerIt = IAssetProvidersMap_.find(assetId.type);
    if (providerIt != IAssetProvidersMap_.end() && providerIt->second->exists(assetId.ticker)) {
        logging::info("load: '{}' fetching from provider", assetId.ticker);
        std::shared_ptr<finance::IAsset> fetched = providerIt->second->fetch(assetId.ticker);
        if (fetched) {
            if (repoIt != IAssetRepositoryMap_.end()) {
                repoIt->second->save(fetched);
            }
            std::shared_ptr<const finance::IAsset> cached = fetched;
            cachedAssets_[assetId] = cached;
            return cached;
        }
    }

    throw Exception("AssetService::load: asset not found in repository or provider for ticker {}.", assetId.ticker);
}

TimeSeries AssetService::loadTimeSeriesValue(const AssetId& assetId, Timestamp startMs, Timestamp endMs,
                                             Timestamp frequencyMs, InterpolationStrategy strategy) {
    if (assetId.type == AssetType::Cash) {
        return constantCashSeries(assetId, startMs, endMs, frequencyMs);
    }

    auto asset = load(assetId);
    const std::string seriesId = asset->priceSeriesId();
    if (seriesId.empty()) {
        return ts::common::utils::timeSeries::generateConstantTimeSeries(
            assetId.ticker, startMs, endMs, frequencyMs, 1.0);
    }
    return timeSeriesService_->getResampled(seriesId, startMs, endMs, frequencyMs, strategy);
}

TimeSeries AssetService::loadTimeSeriesValue(const AssetId& assetId, TimestampsPtr timestamps) {
    ensure<InvalidArgument>(timestamps != nullptr, "AssetService::loadTimeSeriesValue: timestamps pointer is null.");

    if (assetId.type == AssetType::Cash) {
        return constantCashSeries(assetId, std::move(timestamps));
    }

    auto asset = load(assetId);
    const std::string seriesId = asset->priceSeriesId();
    if (seriesId.empty()) {
        return ts::common::utils::timeSeries::generateConstantTimeSeries(assetId.ticker, std::move(timestamps), 1.0);
    }
    // Latest (look-back only) — valuing at t must never use a bar stamped after t.
    return timeSeriesService_->getFilled(seriesId, std::move(timestamps), InterpolationStrategy::Latest);
}

TimestampsPtr AssetService::rawTicks(const AssetId& assetId, Timestamp startMs, Timestamp endMs) {
    // Cash and unpriced assets have no market observations — they contribute no ticks to a grid.
    if (assetId.type == AssetType::Cash) return std::make_shared<std::vector<Timestamp>>();
    auto asset = load(assetId);
    const std::string seriesId = asset->priceSeriesId();
    if (seriesId.empty()) return std::make_shared<std::vector<Timestamp>>();
    const TimeSeries raw = timeSeriesService_->getRaw(seriesId, startMs, endMs);
    const auto span = raw.getTimestamps();
    return std::make_shared<std::vector<Timestamp>>(span.begin(), span.end());
}

double AssetService::loadValueAtTs(const finance::AssetId& assetId, const Timestamp& timestamp) {
    if (assetId.type == AssetType::Cash) {
        return 1.0;
    }
    auto asset = load(assetId);
    const std::string seriesId = asset->priceSeriesId();
    if (seriesId.empty()) {
        return 1.0;
    }
    return timeSeriesService_->getSinglePoint(seriesId, timestamp);
}

std::shared_ptr<TimeSeriesSession> AssetService::createSession(const AssetId& id, Timestamp startMs, Timestamp endMs,
                                                               Timestamp frequencyMs) {
    auto asset = load(id);
    return std::make_shared<TimeSeriesSession>(timeSeriesService_, asset->priceSeriesId(), startMs, endMs, frequencyMs);
}

std::shared_ptr<TimeSeriesSession> AssetService::createSession(const AssetId& id, TimestampsPtr timestamps) {
    auto asset = load(id);
    return std::make_shared<TimeSeriesSession>(timeSeriesService_, asset->priceSeriesId(), std::move(timestamps));
}
}  // namespace finapp
