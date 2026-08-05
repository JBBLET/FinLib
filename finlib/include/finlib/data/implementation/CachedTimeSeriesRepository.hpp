// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finlib/common/Error.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/Log.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/CoverageInfo.hpp"
#include "finlib/data/SeriesKey.hpp"
#include "finlib/data/TimeRange.hpp"
#include "finlib/data/interfaces/ITimeSeriesRepository.hpp"

namespace ts {
class CachedTimeSeriesRepository : public ITimeSeriesRepository {
 public:
    explicit CachedTimeSeriesRepository(std::shared_ptr<ITimeSeriesRepository> inner) : inner_(std::move(inner)) {}

    // --- ITimeSeriesLoader ---

    TimeSeries load(const std::string& id, Timestamp startMs, Timestamp endMs,
                    std::optional<Timestamp> /*requestedFrequency*/ = std::nullopt) const override {
        return inner_->load(id, startMs, endMs);
    }

    LoaderCapabilities capabilities(const std::string& id) const override { return inner_->capabilities(id); }

    // --- ITimeSeriesSaver (via doSave/doMerge) ---

 protected:
    void doSave(const SeriesKey& key, const TimeSeries& ts) override {
        logging::debug("save: {} {} points", key, ts.size());
        inner_->save(key, ts);
        cache_.insert_or_assign(key, ts);
        auto cov = inner_->coverage(key);  // coverage is computed by the inner repo from the data
        if (cov) coverageCache_.insert_or_assign(key, *cov);
    }

    void doMerge(const SeriesKey& key, const TimeSeries& newData) override {
        logging::debug("merge: {} +{} points", key, newData.size());
        inner_->merge(key, newData);
        try {
            cache_.insert_or_assign(key, inner_->load(key));
            auto cov = inner_->coverage(key);
            if (cov) coverageCache_.insert_or_assign(key, *cov);
        } catch (...) {
            cache_.erase(key);
            coverageCache_.erase(key);
        }
    }

 public:
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

    std::vector<Timestamp> availableFrequencies(const std::string& id) const override {
        return inner_->availableFrequencies(id);
    }

    TimeSeries load(const SeriesKey& key) const override {
        if (cache_.contains(key)) {
            logging::debug("memory cache hit: {}", key);
            return cache_.at(key);
        }
        logging::debug("no cache hit, full load: {}", key);
        TimeSeries ts = inner_->load(key);
        auto cov = inner_->coverage(key);
        cache_.insert_or_assign(key, ts);
        if (cov) coverageCache_.insert_or_assign(key, *cov);
        return ts;
    }

    TimeSeries load(const SeriesKey& key, Timestamp startMs, Timestamp endMs) const override {
        if (coverageCache_.contains(key) && cache_.contains(key)) {
            auto gaps = computeGaps(coverageCache_.at(key), TimeRange{startMs, endMs});
            if (gaps.empty()) {
                logging::debug("memory cache hit: {} {}", key, TimeRange{startMs, endMs});
                return filterByRange_(cache_.at(key), startMs, endMs);
            }
        }
        logging::debug("no cache hit, full load: {} {}", key, TimeRange{startMs, endMs});
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

    static TimeSeries filterByRange_(const TimeSeries& full, Timestamp startMs, Timestamp endMs) {
        const auto& timestamps = full.getTimestamps();
        const auto& values = full.getValues();

        auto startIt = std::lower_bound(timestamps.begin(), timestamps.end(), startMs);
        auto endIt = std::upper_bound(timestamps.begin(), timestamps.end(), endMs);

        auto startIdx = static_cast<ptrdiff_t>(std::distance(timestamps.begin(), startIt));
        auto endIdx = static_cast<ptrdiff_t>(std::distance(timestamps.begin(), endIt));

        Timestamps filteredTs(timestamps.begin() + startIdx, timestamps.begin() + endIdx);
        std::vector<double> filteredVals(values.begin() + startIdx, values.begin() + endIdx);

        ensure(!filteredTs.empty(), "no data in {} for {:s}", (TimeRange{startMs, endMs}), full);

        return TimeSeries(full.getId(), std::move(filteredTs), std::move(filteredVals));
    }
};
}  // namespace ts
