// Copyright (c) 2026 JBBLET. All Rights Reserved.
// Test-only fakes shared across service unit tests (FX / Asset / Portfolio).
// In-memory repository implementations live in the production headers under
// finapp/data/repository/implementation/InMemoryRepository/.
#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finapp/data/providers/interfaces/IAssetProviders.hpp"
#include "finapp/data/repository/implementation/InMemoryRepository/InMemoryAssetRepository.hpp"
#include "finapp/data/repository/implementation/InMemoryRepository/InMemoryFXRepository.hpp"
#include "finapp/data/repository/implementation/InMemoryRepository/InMemoryPortfolioRepository.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/implementation/InMemoryTimeSeriesRepository.hpp"
#include "finlib/data/interfaces/ITimeSeriesLoader.hpp"

namespace finapp::test {

using namespace finance;
using namespace finapp;

// ---------------------------------------------------------------------------
// TimeSeries provider fake
// ---------------------------------------------------------------------------

// Fake ITimeSeriesLoader used as the TimeSeriesService provider. Callers preload
// a TimeSeries per id via setSeries(); load() returns the preloaded series filtered
// to the requested range. capabilities() reports daily finest-frequency by default.
class FakeTimeSeriesLoader : public ITimeSeriesLoader {
 public:
    void setSeries(const std::string& id, TimeSeries ts) { series_.insert_or_assign(id, std::move(ts)); }

    TimeSeries load(const std::string& id, int64_t startMs, int64_t endMs) const override {
        auto it = series_.find(id);
        if (it == series_.end()) {
            throw std::runtime_error("FakeTimeSeriesLoader: no series configured for id " + id);
        }
        const auto& full = it->second;
        const auto& timestamps = full.getTimestamps();
        const auto& values = full.getValues();
        std::vector<int64_t> ots;
        std::vector<double> ovs;
        for (size_t i = 0; i < timestamps.size(); ++i) {
            if (timestamps[i] >= startMs && timestamps[i] <= endMs) {
                ots.push_back(timestamps[i]);
                ovs.push_back(values[i]);
            }
        }
        if (ots.empty()) {
            throw std::runtime_error("FakeTimeSeriesLoader: no values in range for " + id);
        }
        return TimeSeries(full.getId(), std::move(ots), std::move(ovs));
    }

    LoaderCapabilities capabilities(const std::string& id) const override {
        (void)id;
        return LoaderCapabilities{0, 86'400'000};
    }

 private:
    std::unordered_map<std::string, TimeSeries> series_;
};

// ---------------------------------------------------------------------------
// Asset provider fake
// ---------------------------------------------------------------------------

class FakeAssetProvider : public IAssetProvider {
 public:
    void setAsset(const std::string& ticker, std::shared_ptr<IAsset> asset) {
        assets_[ticker] = std::move(asset);
    }

    std::shared_ptr<IAsset> fetch(const std::string& ticker) const override {
        auto it = assets_.find(ticker);
        return it == assets_.end() ? nullptr : it->second;
    }

    bool exists(const std::string& ticker) const override { return assets_.contains(ticker); }

 private:
    std::unordered_map<std::string, std::shared_ptr<IAsset>> assets_;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Build a TimeSeries at a regular grid with a constant value — used to preload the
// fake provider with canned market data for tests.
inline TimeSeries makeFlatSeries(const std::string& id, int64_t startMs, int64_t endMs, int64_t frequencyMs,
                                  double value) {
    std::vector<int64_t> ts;
    for (int64_t t = startMs; t <= endMs; t += frequencyMs) ts.push_back(t);
    std::vector<double> vs(ts.size(), value);
    return TimeSeries(id, std::move(ts), std::move(vs));
}

}  // namespace finapp::test
