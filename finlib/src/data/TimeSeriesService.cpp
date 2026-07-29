// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finlib/data/services/TimeSeriesService.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/logger/PrefixedLogger.hpp"
#include "finlib/core/Resampling.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/CoverageInfo.hpp"
#include "finlib/data/SeriesKey.hpp"
#include "finlib/data/TimeRange.hpp"

namespace {
ts::TimeSeries stripNaN(ts::TimeSeries series) {
    const auto stamps = series.getTimestamps();
    const auto& vals = series.getValues();
    std::vector<int64_t> cleanTs;
    std::vector<double> cleanVals;
    cleanTs.reserve(stamps.size());
    cleanVals.reserve(vals.size());
    for (size_t i = 0; i < stamps.size(); ++i) {
        if (!std::isnan(vals[i])) {
            cleanTs.push_back(stamps[i]);
            cleanVals.push_back(vals[i]);
        }
    }
    return ts::TimeSeries(series.getId(), std::move(cleanTs), std::move(cleanVals));
}

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

ts::Timestamp minSpacing(const ts::Timestamps& grid) {
    if (grid.size() < 2) return INT64_MAX;
    ts::Timestamp best = INT64_MAX;
    for (size_t i = 1; i < grid.size(); ++i) {
        const ts::Timestamp d = grid[i] - grid[i - 1];
        if (d > 0 && d < best) best = d;
    }
    return best;
}
}  // namespace

namespace ts {

TimeSeriesService::TimeSeriesService(std::shared_ptr<CachedTimeSeriesRepository> cache,
                                     std::shared_ptr<ITimeSeriesLoader> provider, logging::ILogger* logger)
    : cache_(std::move(cache)),
      provider_(std::move(provider)),
      logger_(logging::PrefixedLogger::wrap(logger, "TimeSeriesService")) {}

std::optional<SeriesKey> TimeSeriesService::selectBucket_(const std::string& id, Timestamp startMs, Timestamp endMs,
                                                          Timestamp coarsestMs, bool finestFirst) const {
    auto freqs = cache_->availableFrequencies(id);
    // finestFirst -> ascending (most points first); else coarsest first (cheapest sufficient).
    if (finestFirst)
        std::sort(freqs.begin(), freqs.end());
    else
        std::sort(freqs.begin(), freqs.end(), std::greater<Timestamp>());
    for (Timestamp freq : freqs) {
        if (freq > coarsestMs) continue;  // too coarse to resolve the grid
        const SeriesKey k{id, freq};
        auto cov = cache_->coverage(k);
        if (cov && computeGaps(*cov, TimeRange{startMs, endMs}).empty()) return k;
    }
    return std::nullopt;
}

TimeSeries TimeSeriesService::loadBucket_(const std::string& id, Timestamp startMs, Timestamp endMs,
                                          Timestamp coarsestMs, bool finestFirst) {
    if (auto key = selectBucket_(id, startMs, endMs, coarsestMs, finestFirst)) {
        return cache_->load(*key, startMs, endMs);
    }

    if (!provider_) {
        throw std::runtime_error("TimeSeriesService::loadBucket_: no provider and no stored series <= " +
                                 std::to_string(coarsestMs) + "ms covering range for '" + id + "'");
    }
    const Timestamp providerFreq = provider_->capabilities(id).frequencyForRange(endMs - startMs);
    if (providerFreq > coarsestMs) {
        throw std::runtime_error(
            "TimeSeriesService::loadBucket_: cannot serve interval <= " + std::to_string(coarsestMs) +
            "ms for this range of '" + id + "' (finest available is " + std::to_string(providerFreq) + "ms)");
    }

    const SeriesKey key{id, providerFreq};
    if (cache_->exists(key)) {
        // Existing bucket — extend it at its boundaries only.
        if (auto cov = cache_->coverage(key)) {
            auto gaps = computeGaps(*cov, TimeRange{startMs, endMs});
            if (!gaps.empty()) {
                if (logger_)
                    logger_->write(logging::Level::Debug,
                                   "loadBucket_ '" + id + "' freq=" + std::to_string(providerFreq) + "ms: filling " +
                                       std::to_string(gaps.size()) + " boundary gap(s)");
                fetchAndMergeGaps_(key, gaps);
            }
        }
    } else {
        if (logger_)
            logger_->write(logging::Level::Info,
                           "loadBucket_ '" + id + "' freq=" + std::to_string(providerFreq) +
                               "ms: provider full fetch [" + std::to_string(startMs) + ", " + std::to_string(endMs) +
                               "]");
        TimeSeries fetched = stripNaN(provider_->load(id, startMs, endMs, providerFreq));
        if (fetched.size() == 0) {
            throw std::runtime_error("TimeSeriesService::loadBucket_: provider returned no data for '" + id + "'");
        }
        cache_->save(key, fetched);  // persists down to the DB (coverage computed on read)
    }
    return cache_->load(key, startMs, endMs);
}

TimeSeries TimeSeriesService::getRaw(const std::string& id, Timestamp startMs, Timestamp endMs, Timestamp coarsestMs) {
    // Finest available (most points) within the optional cap.
    return loadBucket_(id, startMs, endMs, coarsestMs, /*finestFirst=*/true);
}

TimeSeries TimeSeriesService::getAligned(const std::string& id, TimestampsPtr grid) {
    if (!grid || grid->empty()) {
        throw std::invalid_argument("TimeSeriesService::getAligned: grid must be non-empty.");
    }
    // Analysis: coarsest bucket fine enough to resolve the grid (else throw — no fabrication).
    TimeSeries bucket = loadBucket_(id, grid->front(), grid->back(), minSpacing(*grid), /*finestFirst=*/false);
    return resample(bucket, std::move(grid), InterpolationStrategy::Exact);
}

TimeSeries TimeSeriesService::getFilled(const std::string& id, TimestampsPtr grid, InterpolationStrategy strategy) {
    if (!grid || grid->empty()) {
        throw std::invalid_argument("TimeSeriesService::getFilled: grid must be non-empty.");
    }
    // Graphing: coarsest available bucket (interpolation may upsample from coarser data).
    TimeSeries bucket = loadBucket_(id, grid->front(), grid->back(), INT64_MAX, /*finestFirst=*/false);
    return resample(bucket, std::move(grid), strategy);
}

TimeSeries TimeSeriesService::getFilled(const std::string& id, Timestamp startMs, Timestamp endMs, Timestamp freqMs,
                                        InterpolationStrategy strategy) {
    auto grid = std::make_shared<Timestamps>();
    for (Timestamp t = startMs; t <= endMs; t += freqMs) grid->push_back(t);
    if (grid->empty()) grid->push_back(startMs);
    return getFilled(id, std::move(grid), strategy);
}

double TimeSeriesService::getSinglePoint(const std::string& id, Timestamp ts) { return singlePoint_(id, ts, false); }
double TimeSeriesService::getSinglePointOrThrow(const std::string& id, Timestamp ts) {
    return singlePoint_(id, ts, true);
}

double TimeSeriesService::singlePoint_(const std::string& id, Timestamp ts, bool requireExact) {
    // 1. Exact hit in any cached series for this id.
    for (Timestamp freq : cache_->availableFrequencies(id)) {
        const SeriesKey k{id, freq};
        if (!cache_->exists(k)) continue;
        try {
            auto exact = cache_->load(k).exactValue(ts);
            if (exact.has_value() && !std::isnan(*exact)) return *exact;
        } catch (...) {
        }
    }

    if (provider_) {
        const auto caps = provider_->capabilities(id);
        const Timestamp ageMs = (ts < nowMs()) ? (nowMs() - ts) : 0LL;
        const Timestamp freqMs = caps.frequencyForRange(ageMs);
        const Timestamp minRange = caps.minFetchRangeFor(ageMs);
        const Timestamp windowMs = std::max((minRange / 2) + 1, 5 * freqMs);

        TimeSeries raw = provider_->load(id, ts - windowMs, ts + windowMs);
        if (raw.size() > 0) {
            auto providerExact = raw.exactValue(ts);
            TimeSeries clean = stripNaN(std::move(raw));
            if (clean.size() > 0) cache_->merge(SeriesKey{id, freqMs}, clean);
            if (providerExact.has_value() && !std::isnan(*providerExact)) return *providerExact;
            if (!requireExact && clean.size() > 0) return clean.latestValue(ts);
        }
    }

    if (!requireExact) {
        auto freqs = cache_->availableFrequencies(id);
        std::sort(freqs.begin(), freqs.end());
        for (Timestamp freq : freqs) {
            const SeriesKey k{id, freq};
            auto cov = cache_->coverage(k);
            if (!cov || cov->coveredFromMs > ts) continue;
            try {
                double v = cache_->load(k).latestValue(ts);
                if (!std::isnan(v)) return v;
            } catch (...) {
            }
        }
    }

    throw std::runtime_error("TimeSeriesService::singlePoint_: no " + std::string(requireExact ? "exact " : "") +
                             "data for '" + id + "' at ts=" + std::to_string(ts));
}

TimeSeries TimeSeriesService::get(const std::string& id, Timestamp startMs, Timestamp endMs, Timestamp freqMs) {
    return getFilled(id, startMs, endMs, freqMs, InterpolationStrategy::Nearest);
}

TimeSeries TimeSeriesService::getResampled(const std::string& id, Timestamp startMs, Timestamp endMs, Timestamp freqMs,
                                           InterpolationStrategy strategy) {
    return getFilled(id, startMs, endMs, freqMs, strategy);
}

TimeSeries TimeSeriesService::get(const std::string& id, TimestampsPtr grid) {
    return getFilled(id, std::move(grid), InterpolationStrategy::Nearest);
}

void TimeSeriesService::fetchAndMergeGaps_(const SeriesKey& key, const std::vector<TimeRange>& gaps) {
    if (!provider_) {
        throw std::runtime_error("No provider available to fetch gaps for series: " + key.SeriesId);
    }
    for (const auto& gap : gaps) {
        // Skip gaps narrower than the series interval — nothing new to fetch there.
        if (gap.endTimeStampMs - gap.startTimeStampMs < key.frequencyInMs) continue;
        TimeSeries gapData =
            stripNaN(provider_->load(key.SeriesId, gap.startTimeStampMs, gap.endTimeStampMs, key.frequencyInMs));
        if (gapData.size() > 0) cache_->merge(key, gapData);
    }
}

}  // namespace ts
