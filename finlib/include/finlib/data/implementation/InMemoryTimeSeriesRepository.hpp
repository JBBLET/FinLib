// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/CoverageInfo.hpp"
#include "finlib/data/SeriesKey.hpp"
#include "finlib/data/interfaces/ITimeSeriesRepository.hpp"

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
    void doSave(const SeriesKey& key, const TimeSeries& ts, const CoverageInfo& cov) override {
        data_.insert_or_assign(key, ts);
        coverage_.insert_or_assign(key, cov);
    }

    void doMerge(const SeriesKey& key, const TimeSeries& newData) override {
        auto it = data_.find(key);
        if (it == data_.end()) {
            data_.emplace(key, newData);
            return;
        }
        it->second = newData;
    }

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
