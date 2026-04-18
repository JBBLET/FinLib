// Copyright (c) 2026 JBBLET. All Rights Reserved.
// In-memory fakes shared across service unit tests (FX / Asset / Portfolio).
#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finapp/data/providers/interfaces/IAssetProviders.hpp"
#include "finapp/data/repository/interface/IAssetRepository.hpp"
#include "finapp/data/repository/interface/IFXRepository.hpp"
#include "finapp/data/repository/interface/IPortfolioRepository.hpp"
#include "finapp/finance/asset/IAsset.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finapp/finance/portfolio/PortfolioSnapshot.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/CoverageInfo.hpp"
#include "finlib/data/SeriesKey.hpp"
#include "finlib/data/interfaces/ITimeSeriesLoader.hpp"
#include "finlib/data/interfaces/ITimeSeriesRepository.hpp"

namespace finapp::test {

using namespace finance;
using namespace finapp;

// ---------------------------------------------------------------------------
// TimeSeries provider/repository fakes
// ---------------------------------------------------------------------------

// Minimal in-memory ITimeSeriesRepository. Stores whatever the service saves,
// reports coverage exactly over what was saved. Does not fetch on load misses.
class InMemoryTimeSeriesRepository : public ITimeSeriesRepository {
 public:
    TimeSeries load(const std::string& id, int64_t startMs, int64_t endMs) const override {
        auto it = std::find_if(data_.begin(), data_.end(),
                               [&](const auto& kv) { return kv.first.SeriesId == id; });
        if (it == data_.end()) {
            throw std::runtime_error("InMemoryTimeSeriesRepository: no data for id " + id);
        }
        return filter_(it->second, startMs, endMs);
    }

    LoaderCapabilities capabilities(const std::string& id) const override {
        (void)id;
        return LoaderCapabilities{0, 86'400'000};
    }

    void save(const SeriesKey& key, const TimeSeries& ts, const CoverageInfo& cov) override {
        data_.insert_or_assign(key, ts);
        coverage_.insert_or_assign(key, cov);
    }

    void merge(const SeriesKey& key, const TimeSeries& newData) override {
        auto it = data_.find(key);
        if (it == data_.end()) {
            data_.emplace(key, newData);
            return;
        }
        it->second = newData;
    }

    bool exists(const SeriesKey& key) const override { return data_.contains(key); }

    std::optional<CoverageInfo> coverage(const SeriesKey& key) const override {
        auto it = coverage_.find(key);
        if (it == coverage_.end()) return std::nullopt;
        return it->second;
    }

    std::vector<int64_t> availableFrequencies(const std::string& id) const override {
        std::vector<int64_t> out;
        for (const auto& [key, _] : data_) {
            if (key.SeriesId == id) out.push_back(key.frequencyInMs);
        }
        return out;
    }

    TimeSeries load(const SeriesKey& key) const override {
        auto it = data_.find(key);
        if (it == data_.end()) {
            throw std::runtime_error("InMemoryTimeSeriesRepository: missing key " + key.SeriesId);
        }
        return it->second;
    }

    TimeSeries load(const SeriesKey& key, int64_t startMs, int64_t endMs) const override {
        return filter_(load(key), startMs, endMs);
    }

 private:
    std::unordered_map<SeriesKey, TimeSeries> data_;
    std::unordered_map<SeriesKey, CoverageInfo> coverage_;

    static TimeSeries filter_(const TimeSeries& ts, int64_t startMs, int64_t endMs) {
        const auto& timestamps = ts.getTimestamps();
        const auto& values = ts.getValues();
        std::vector<int64_t> ots;
        std::vector<double> ovs;
        for (size_t i = 0; i < timestamps.size(); ++i) {
            if (timestamps[i] >= startMs && timestamps[i] <= endMs) {
                ots.push_back(timestamps[i]);
                ovs.push_back(values[i]);
            }
        }
        if (ots.empty()) {
            throw std::runtime_error("InMemoryTimeSeriesRepository: empty range for " + ts.getId());
        }
        return TimeSeries(ts.getId(), std::move(ots), std::move(ovs));
    }
};

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
// Domain repository fakes
// ---------------------------------------------------------------------------

class InMemoryFXRepository : public IFXRepository {
 public:
    FXInfos load(const Currency& base, const Currency& quote) const override {
        auto it = entries_.find(key_(base, quote));
        if (it == entries_.end()) {
            throw std::runtime_error("InMemoryFXRepository: pair not found");
        }
        return it->second;
    }

    void save(const FXInfos& info) override { entries_.insert_or_assign(key_(info.baseCurrency, info.quoteCurrency), info); }

    bool exists(const Currency& base, const Currency& quote) const override {
        return entries_.contains(key_(base, quote));
    }

    size_t size() const { return entries_.size(); }

 private:
    static std::string key_(Currency base, Currency quote) { return toString(base) + "/" + toString(quote); }
    std::unordered_map<std::string, FXInfos> entries_;
};

class InMemoryAssetRepository : public IAssetRepository {
 public:
    void save(const std::shared_ptr<const IAsset>& asset) override {
        if (!asset) throw std::invalid_argument("InMemoryAssetRepository: null asset");
        assets_[asset->ticker()] = asset;
    }

    std::shared_ptr<const IAsset> load(const std::string& ticker) const override {
        auto it = assets_.find(ticker);
        return it == assets_.end() ? nullptr : it->second;
    }

    bool exists(const std::string& ticker) const override { return assets_.contains(ticker); }

    std::vector<std::string> listTickers() const override {
        std::vector<std::string> out;
        out.reserve(assets_.size());
        for (const auto& [t, _] : assets_) out.push_back(t);
        return out;
    }

    std::unordered_map<std::string, std::shared_ptr<const IAsset>> loadAll(
        const std::vector<std::string>& tickers) const override {
        std::unordered_map<std::string, std::shared_ptr<const IAsset>> out;
        for (const auto& t : tickers) {
            auto it = assets_.find(t);
            if (it != assets_.end()) out.emplace(t, it->second);
        }
        return out;
    }

    size_t count() const { return assets_.size(); }

 private:
    std::unordered_map<std::string, std::shared_ptr<const IAsset>> assets_;
};

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

class InMemoryPortfolioRepository : public IPortfolioRepository {
 public:
    void saveSnapshot(const PortfolioSnapshot& snapshot) override {
        snapshots_[snapshot.portfolioId] = snapshot;
    }

    std::optional<PortfolioSnapshot> loadLatestSnapshot(const std::string& portfolioId) const override {
        auto it = snapshots_.find(portfolioId);
        if (it == snapshots_.end()) return std::nullopt;
        return it->second;
    }

    void appendTransactions(const std::string& portfolioId,
                            const std::vector<Transaction>& transactions) override {
        auto& existing = transactions_[portfolioId];
        existing.insert(existing.end(), transactions.begin(), transactions.end());
    }

    std::vector<Transaction> loadTransactions(const std::string& portfolioId,
                                              int64_t afterTimestamps = 0) const override {
        auto it = transactions_.find(portfolioId);
        if (it == transactions_.end()) return {};
        std::vector<Transaction> out;
        for (const auto& tx : it->second) {
            if (tx.timestampsMs >= afterTimestamps) out.push_back(tx);
        }
        return out;
    }

    std::vector<std::string> listPortfolioIds() const override {
        std::vector<std::string> out;
        for (const auto& [id, _] : snapshots_) out.push_back(id);
        return out;
    }

    bool exists(const std::string& portfolioId) const override {
        return snapshots_.contains(portfolioId) || transactions_.contains(portfolioId);
    }

    void deletePortfolio(const std::string& portfolioId) override {
        if (!exists(portfolioId)) {
            throw std::runtime_error("InMemoryPortfolioRepository: portfolio not found: " + portfolioId);
        }
        snapshots_.erase(portfolioId);
        transactions_.erase(portfolioId);
    }

 private:
    std::unordered_map<std::string, PortfolioSnapshot> snapshots_;
    std::unordered_map<std::string, std::vector<Transaction>> transactions_;
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
