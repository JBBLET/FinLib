// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/CoverageInfo.hpp"
#include "finlib/data/SeriesKey.hpp"
#include "finlib/data/TimeRange.hpp"
#include "finlib/data/interfaces/ITimeSeriesRepository.hpp"

class CachedTimeSeriesRepository : public ITimeSeriesRepository {
 public:
    explicit CachedTimeSeriesRepository(std::shared_ptr<ITimeSeriesRepository> inner)
        : inner_(std::move(inner)) {}

    // --- ITimeSeriesLoader ---

    TimeSeries load(const std::string& id, int64_t startMs, int64_t endMs) const override {
        return inner_->load(id, startMs, endMs);
    }

    LoaderCapabilities capabilities(const std::string& id) const override {
        return inner_->capabilities(id);
    }

    // --- ITimeSeriesSaver ---

    void save(const SeriesKey& key, const TimeSeries& ts, const CoverageInfo& cov) override {
        inner_->save(key, ts, cov);
        cache_.insert_or_assign(key, ts);
        coverageCache_.insert_or_assign(key, cov);
    }

    void merge(const SeriesKey& key, const TimeSeries& newData) override {
        inner_->merge(key, newData);
        // Repopulate cache from the merged result in inner
        try {
            cache_.insert_or_assign(key, inner_->load(key));
            auto cov = inner_->coverage(key);
            if (cov) coverageCache_.insert_or_assign(key, *cov);
        } catch (...) {
            cache_.erase(key);
            coverageCache_.erase(key);
        }
    }

    // --- ITimeSeriesRepository ---

    bool exists(const SeriesKey& key) const override {
        if (cache_.contains(key)) return true;
        return inner_->exists(key);
    }

    std::optional<CoverageInfo> coverage(const SeriesKey& key) const override {
        if (coverageCache_.contains(key)) return coverageCache_.at(key);
        auto cov = inner_->coverage(key);
        if (cov) coverageCache_.insert_or_assign(key, *cov);
        return cov;
    }

    std::vector<int64_t> availableFrequencies(const std::string& id) const override {
        return inner_->availableFrequencies(id);
    }

    TimeSeries load(const SeriesKey& key) const override {
        if (cache_.contains(key)) return cache_.at(key);
        TimeSeries ts = inner_->load(key);
        auto cov = inner_->coverage(key);
        cache_.insert_or_assign(key, ts);
        if (cov) coverageCache_.insert_or_assign(key, *cov);
        return ts;
    }

    TimeSeries load(const SeriesKey& key, int64_t startMs, int64_t endMs) const override {
        // Check if cache fully covers the requested range
        if (coverageCache_.contains(key) && cache_.contains(key)) {
            auto gaps = computeGaps(coverageCache_.at(key), TimeRange{startMs, endMs});
            if (gaps.empty()) {
                return filterByRange_(cache_.at(key), startMs, endMs);
            }
        }
        // Cache miss or insufficient — load full from inner, populate cache, filter
        TimeSeries full = inner_->load(key);
        auto cov = inner_->coverage(key);
        cache_.insert_or_assign(key, full);
        if (cov) coverageCache_.insert_or_assign(key, *cov);
        return filterByRange_(full, startMs, endMs);
    }

 private:
    std::shared_ptr<ITimeSeriesRepository> inner_;
    mutable std::unordered_map<SeriesKey, TimeSeries> cache_;
    mutable std::unordered_map<SeriesKey, CoverageInfo> coverageCache_;

    static TimeSeries filterByRange_(const TimeSeries& full, int64_t startMs, int64_t endMs) {
        const auto& timestamps = full.getTimestamps();
        const auto& values = full.getValues();

        auto startIt = std::lower_bound(timestamps.begin(), timestamps.end(), startMs);
        auto endIt = std::upper_bound(timestamps.begin(), timestamps.end(), endMs);

        auto startIdx = static_cast<ptrdiff_t>(std::distance(timestamps.begin(), startIt));
        auto endIdx = static_cast<ptrdiff_t>(std::distance(timestamps.begin(), endIt));

        std::vector<int64_t> filteredTs(timestamps.begin() + startIdx, timestamps.begin() + endIdx);
        std::vector<double> filteredVals(values.begin() + startIdx, values.begin() + endIdx);

        if (filteredTs.empty()) {
            throw std::runtime_error("No data found in range for series: " + full.getId());
        }

        return TimeSeries(full.getId(), std::move(filteredTs), std::move(filteredVals));
    }
};
