// Copyright (c) 2026 JBBLET. All Rights Reserved.

#include "finapp/service/AssetService.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/asset/Cash.hpp"
#include "finapp/common/logger/PrefixedLogger.hpp"
#include "finlib/common/utils/TimeSeriesUtils.hpp"
#include "finlib/core/TimeSeries.hpp"

namespace finapp {

using namespace finance;

namespace {

// Cash assets have no price series — they are their own numeraire. Return a
// constant 1.0 series in the cash's denomination; FX conversion is the caller's job.
TimeSeries constantCashSeries(const AssetId& assetId, int64_t startMs, int64_t endMs, int64_t frequencyMs) {
    return common::utils::timeSeries::generateConstantTimeSeries(assetId.ticker, startMs, endMs, frequencyMs, 1.0);
}

TimeSeries constantCashSeries(const AssetId& assetId, TimestampPtr timestamps) {
    return common::utils::timeSeries::generateConstantTimeSeries(assetId.ticker, std::move(timestamps), 1.0);
}

}  // namespace

AssetService::AssetService(std::shared_ptr<TimeSeriesService> timeSeriesService,
                           std::unordered_map<AssetType, std::shared_ptr<IAssetRepository>> IAssetRepositoryMap,
                           std::unordered_map<AssetType, std::shared_ptr<IAssetProvider>> IAssetProvidersMap,
                           finapp::logging::ILogger* logger)
    : timeSeriesService_(std::move(timeSeriesService)),
      IAssetRepositoryMap_(std::move(IAssetRepositoryMap)),
      IAssetProvidersMap_(std::move(IAssetProvidersMap)),
      logger_(finapp::logging::PrefixedLogger::wrap(logger, "AssetService")) {}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void AssetService::save(const std::shared_ptr<IAsset>& asset) {
    if (!asset) {
        throw std::invalid_argument("AssetService::save: asset pointer is null.");
    }

    auto repoIt = IAssetRepositoryMap_.find(asset->type());
    if (repoIt == IAssetRepositoryMap_.end()) {
        throw std::runtime_error("AssetService::save: no repository registered for asset type " +
                                 assetTypeToString(asset->type()));
    }

    repoIt->second->save(asset);
    cachedAssets_[AssetId{asset->type(), asset->ticker()}] = asset;
}

std::shared_ptr<const IAsset> AssetService::load(const AssetId& assetId) {
    // 1. In-memory cache hit.
    if (auto it = cachedAssets_.find(assetId); it != cachedAssets_.end()) {
        return it->second;
    }

    // Cash is synthesized on demand — it has no repository row and no provider.
    if (assetId.type == AssetType::Cash) {
        auto cash = std::make_shared<const Cash>(currencyFromString(assetId.ticker.substr(0, 3)));
        cachedAssets_[assetId] = cash;
        return cash;
    }

    // 2. Repository hit.
    auto repoIt = IAssetRepositoryMap_.find(assetId.type);
    if (repoIt != IAssetRepositoryMap_.end() && repoIt->second->exists(assetId.ticker)) {
        auto asset = repoIt->second->load(assetId.ticker);
        if (asset) {
            if (logger_) logger_->write(finapp::logging::Level::Debug, "load: '" + assetId.ticker + "' from repository");
            cachedAssets_[assetId] = asset;
            return asset;
        }
    }

    // 3. Provider fallback — persist back into the repository so next call hits step 2.
    auto providerIt = IAssetProvidersMap_.find(assetId.type);
    if (providerIt != IAssetProvidersMap_.end() && providerIt->second->exists(assetId.ticker)) {
        if (logger_) logger_->write(finapp::logging::Level::Info, "load: '" + assetId.ticker + "' fetching from provider");
        std::shared_ptr<IAsset> fetched = providerIt->second->fetch(assetId.ticker);
        if (fetched) {
            if (repoIt != IAssetRepositoryMap_.end()) {
                repoIt->second->save(fetched);
            }
            std::shared_ptr<const IAsset> cached = fetched;
            cachedAssets_[assetId] = cached;
            return cached;
        }
    }

    throw std::runtime_error("AssetService::load: asset not found in repository or provider for ticker " +
                             assetId.ticker);
}

TimeSeries AssetService::loadTimeSeriesValue(const AssetId& assetId, int64_t startMs, int64_t endMs,
                                             int64_t frequencyMs, InterpolationStrategy strategy) {
    if (assetId.type == AssetType::Cash) {
        return constantCashSeries(assetId, startMs, endMs, frequencyMs);
    }

    auto asset = load(assetId);
    const std::string seriesId = asset->priceSeriesId();
    if (seriesId.empty()) {
        return common::utils::timeSeries::generateConstantTimeSeries(assetId.ticker, startMs, endMs, frequencyMs, 1.0);
    }
    return timeSeriesService_->getResampled(seriesId, startMs, endMs, frequencyMs, strategy);
}

TimeSeries AssetService::loadTimeSeriesValue(const AssetId& assetId, TimestampPtr timestamps) {
    if (!timestamps) {
        throw std::invalid_argument("AssetService::loadTimeSeriesValue: timestamps pointer is null.");
    }

    if (assetId.type == AssetType::Cash) {
        return constantCashSeries(assetId, std::move(timestamps));
    }

    auto asset = load(assetId);
    const std::string seriesId = asset->priceSeriesId();
    if (seriesId.empty()) {
        return common::utils::timeSeries::generateConstantTimeSeries(assetId.ticker, std::move(timestamps), 1.0);
    }
    return timeSeriesService_->get(seriesId, std::move(timestamps));
}

}  // namespace finapp
