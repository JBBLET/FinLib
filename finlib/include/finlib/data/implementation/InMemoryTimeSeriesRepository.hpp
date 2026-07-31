// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <algorithm>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "finlib/common/Error.hpp"
#include "finlib/common/FinlibTypes.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/CoverageInfo.hpp"
#include "finlib/data/SeriesKey.hpp"
#include "finlib/data/interfaces/ITimeSeriesRepository.hpp"

namespace ts {
// Minimal in-memory ITimeSeriesRepository. Stores whatever the service saves; coverage is computed
// from the stored timestamps (the extent). Does not fetch on load misses.
class InMemoryTimeSeriesRepository : public ITimeSeriesRepository {
 public:
    TimeSeries load(const std::string& id, Timestamp startMs, Timestamp endMs,
                    std::optional<Timestamp> /*requestedFrequency*/ = std::nullopt) const override {
        auto it = std::find_if(data_.begin(), data_.end(), [&](const auto& kv) { return kv.first.SeriesId == id; });
        ensure(it != data_.end(), "InMemoryTimeSeriesRepository: no data for id {}", id);
        return filter_(it->second, startMs, endMs);
    }

    LoaderCapabilities capabilities(const std::string& id) const override {
        (void)id;
        return LoaderCapabilities{0, 86'400'000};
    }

    bool exists(const SeriesKey& key) const override { return data_.contains(key); }

    std::optional<CoverageInfo> coverage(const SeriesKey& key) const override {
        auto it = data_.find(key);
        if (it == data_.end() || it->second.size() == 0) return std::nullopt;
        const auto stamps = it->second.getTimestamps();
        return CoverageInfo{key, stamps.front(), stamps.back(), "computed", 0};
    }

    std::vector<Timestamp> availableFrequencies(const std::string& id) const override {
        std::vector<Timestamp> out;
        for (const auto& [key, _] : data_) {
            if (key.SeriesId == id) out.push_back(key.frequencyInMs);
        }
        return out;
    }

    TimeSeries load(const SeriesKey& key) const override {
        auto it = data_.find(key);
        ensure(it != data_.end(), "InMemoryTimeSeriesRepository: missing key {}", key.SeriesId);
        return it->second;
    }

    TimeSeries load(const SeriesKey& key, Timestamp startMs, Timestamp endMs) const override {
        return filter_(load(key), startMs, endMs);
    }

 private:
    void doSave(const SeriesKey& key, const TimeSeries& ts) override { data_.insert_or_assign(key, ts); }

    // Real union: combine existing + new points, dedupe by timestamp (new wins), keep sorted.
    void doMerge(const SeriesKey& key, const TimeSeries& newData) override {
        if (newData.size() == 0) return;

        std::map<Timestamp, double> combined;
        auto it = data_.find(key);
        if (it != data_.end()) {
            const auto stamps = it->second.getTimestamps();
            const auto& vals = it->second.getValues();
            for (size_t i = 0; i < it->second.size(); ++i) combined[stamps[i]] = vals[i];
        }
        const auto newStamps = newData.getTimestamps();
        const auto& newVals = newData.getValues();
        for (size_t i = 0; i < newData.size(); ++i) combined[newStamps[i]] = newVals[i];

        Timestamps mergedTs;
        std::vector<double> mergedVals;
        mergedTs.reserve(combined.size());
        mergedVals.reserve(combined.size());
        for (const auto& [t, v] : combined) {
            mergedTs.push_back(t);
            mergedVals.push_back(v);
        }
        data_.insert_or_assign(key, TimeSeries(key.SeriesId, std::move(mergedTs), std::move(mergedVals)));
    }

    std::unordered_map<SeriesKey, TimeSeries> data_;

    static TimeSeries filter_(const TimeSeries& ts, Timestamp startMs, Timestamp endMs) {
        const auto& timestamps = ts.getTimestamps();
        const auto& values = ts.getValues();
        Timestamps ots;
        std::vector<double> ovs;
        for (size_t i = 0; i < timestamps.size(); ++i) {
            if (timestamps[i] >= startMs && timestamps[i] <= endMs) {
                ots.push_back(timestamps[i]);
                ovs.push_back(values[i]);
            }
        }
        ensure(!ots.empty(), "InMemoryTimeSeriesRepository: empty range for {}", ts.getId());
        return TimeSeries(ts.getId(), std::move(ots), std::move(ovs));
    }
};
}  // namespace ts
