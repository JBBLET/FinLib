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

#include "finlib/common/logger/PrefixedLogger.hpp"
#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/CoverageInfo.hpp"
#include "finlib/data/SeriesKey.hpp"
#include "finlib/data/TimeRange.hpp"

namespace {
// Remove NaN entries from a series so they are never stored in or returned from the cache.
TimeSeries stripNaN(TimeSeries ts) {
    const auto& stamps = ts.getTimestamps();
    const auto& vals = ts.getValues();
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
    return TimeSeries(ts.getId(), std::move(cleanTs), std::move(cleanVals));
}
}  // namespace

TimeSeriesService::TimeSeriesService(std::shared_ptr<CachedTimeSeriesRepository> cache,
                                     std::shared_ptr<ITimeSeriesLoader> provider, logging::ILogger* logger)
    : cache_(std::move(cache)),
      provider_(std::move(provider)),
      logger_(logging::PrefixedLogger::wrap(logger, "TimeSeriesService")) {}

// Public API

TimeSeries TimeSeriesService::get(const std::string& id, int64_t startMs, int64_t endMs, int64_t requestedFrequencyMs) {
    SeriesKey requestedKey{id, requestedFrequencyMs};

    // --- Step 1: exact key exists and covers the full range? ---
    if (cache_->exists(requestedKey)) {
        auto cov = cache_->coverage(requestedKey);
        if (cov) {
            auto gaps = computeGaps(*cov, TimeRange{startMs, endMs});
            if (gaps.empty()) {
                if (logger_)
                    logger_->write(logging::Level::Debug,
                                   "get '" + id + "' freq=" + std::to_string(requestedFrequencyMs) +
                                       "ms: cache hit");
                return cache_->load(requestedKey, startMs, endMs);
            }
            if (logger_)
                logger_->write(logging::Level::Debug,
                               "get '" + id + "' freq=" + std::to_string(requestedFrequencyMs) + "ms: partial cache, filling " +
                                   std::to_string(gaps.size()) + " gap(s)");
            fetchAndMergeGaps_(requestedKey, gaps);
            return cache_->load(requestedKey, startMs, endMs);
        }
    }

    // --- Step 2: finer frequency available locally that covers the range? ---
    auto localKey = findLocalCoveringKey_(id, startMs, endMs, requestedFrequencyMs);
    if (localKey) {
        if (logger_)
            logger_->write(logging::Level::Debug,
                           "get '" + id + "' freq=" + std::to_string(requestedFrequencyMs) +
                               "ms: local resample from freq=" + std::to_string(localKey->frequencyInMs) + "ms");
        TimeSeries finerData = cache_->load(*localKey, startMs, endMs);
        if (localKey->frequencyInMs < requestedFrequencyMs) {
            std::vector<int64_t> targetTimestamps;
            for (int64_t t = startMs; t <= endMs; t += requestedFrequencyMs) {
                targetTimestamps.push_back(t);
            }
            return finerData.resampling(targetTimestamps, InterpolationStrategy::Nearest);
        }
        return finerData;
    }

    // --- Step 3: need to fetch from provider ---
    if (!provider_) {
        throw std::runtime_error("No provider available and no local data for series: " + id);
    }

    auto caps = provider_->capabilities(id);
    if (caps.finestFrequencyMs > requestedFrequencyMs) {
        throw std::runtime_error("Requested frequency (" + std::to_string(requestedFrequencyMs) +
                                 " ms) is finer than provider's finest available (" +
                                 std::to_string(caps.finestFrequencyMs) + " ms) for series: " + id);
    }

    // Check partial coverage — fetch only the gaps
    if (cache_->exists(requestedKey)) {
        auto cov = cache_->coverage(requestedKey);
        if (cov) {
            auto gaps = computeGaps(*cov, TimeRange{startMs, endMs});
            if (!gaps.empty()) {
                if (logger_)
                    logger_->write(logging::Level::Info,
                                   "get '" + id + "' freq=" + std::to_string(requestedFrequencyMs) +
                                       "ms: provider gap fill " + std::to_string(gaps.size()) + " gap(s)");
                fetchAndMergeGaps_(requestedKey, gaps);
                return cache_->load(requestedKey, startMs, endMs);
            }
        }
    }

    // No existing data at all — full fetch
    if (logger_)
        logger_->write(logging::Level::Info,
                       "get '" + id + "' freq=" + std::to_string(requestedFrequencyMs) +
                           "ms: provider full fetch [" + std::to_string(startMs) + ", " + std::to_string(endMs) + "]");
    TimeSeries fetched = stripNaN(provider_->load(id, startMs, endMs));
    if (fetched.size() == 0) {
        throw std::runtime_error("TimeSeriesService::get: no data returned by provider for series '" + id +
                                 "' (ticker may be delisted or unavailable)");
    }

    int64_t nowMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();

    CoverageInfo cov{requestedKey, fetched.getTimestamps().front(), fetched.getTimestamps().back(), "provider", nowMs};

    cache_->save(requestedKey, fetched, cov);
    return fetched;
}

TimeSeries TimeSeriesService::getResampled(const std::string& id, int64_t startMs, int64_t endMs,
                                           int64_t requestedFrequencyMs, InterpolationStrategy strategy) {
    // ensureAndResolveKey_ runs the same coverage/gap/fetch logic as get(), then returns
    // the best available SeriesKey (exact match or a finer-frequency one).
    auto ensureAndResolveKey = [&]() -> SeriesKey {
        SeriesKey key{id, requestedFrequencyMs};

        if (cache_->exists(key)) {
            auto cov = cache_->coverage(key);
            if (cov) {
                auto gaps = computeGaps(*cov, TimeRange{startMs, endMs});
                if (!gaps.empty()) fetchAndMergeGaps_(key, gaps);
                return key;
            }
        }

        if (auto localKey = findLocalCoveringKey_(id, startMs, endMs, requestedFrequencyMs)) {
            return *localKey;
        }

        if (!provider_) throw std::runtime_error("No provider available and no local data for series: " + id);

        auto caps = provider_->capabilities(id);
        if (caps.finestFrequencyMs > requestedFrequencyMs)
            throw std::runtime_error("Requested frequency (" + std::to_string(requestedFrequencyMs) +
                                     " ms) is finer than provider's finest available (" +
                                     std::to_string(caps.finestFrequencyMs) + " ms) for series: " + id);

        if (cache_->exists(key)) {
            auto cov = cache_->coverage(key);
            if (cov) {
                auto gaps = computeGaps(*cov, TimeRange{startMs, endMs});
                if (!gaps.empty()) fetchAndMergeGaps_(key, gaps);
                return key;
            }
        }

        TimeSeries fetched = stripNaN(provider_->load(id, startMs, endMs));
        if (fetched.size() == 0)
            throw std::runtime_error("TimeSeriesService::getResampled: no data returned by provider for series '" + id +
                                     "' (ticker may be delisted or unavailable)");

        int64_t nowMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        CoverageInfo cov{key, fetched.getTimestamps().front(), fetched.getTimestamps().back(), "provider", nowMs};
        cache_->save(key, fetched, cov);
        return key;
    };

    const SeriesKey resolvedKey = ensureAndResolveKey();

    // Load full NaN-free series and resample to a regular grid with the requested strategy.
    // This fills weekends, holidays, and any tail beyond the last published data point.
    TimeSeries full = cache_->load(resolvedKey);
    std::vector<int64_t> grid;
    for (int64_t t = startMs; t <= endMs; t += requestedFrequencyMs) grid.push_back(t);
    if (grid.empty()) grid.push_back(startMs);
    return full.resampling(grid, strategy);
}

TimeSeries TimeSeriesService::get(const std::string& id, TimestampPtr timestamps) {
    if (!timestamps || timestamps->empty()) {
        throw std::invalid_argument("TimeSeriesService::get: timestamps must be non-empty.");
    }
    const auto& ts = *timestamps;
    if (!std::is_sorted(ts.begin(), ts.end())) {
        throw std::invalid_argument("TimeSeriesService::get: timestamps must be sorted.");
    }

    int64_t startMs = ts.front();
    int64_t endMs = ts.back();

    // Infer frequency from the grid and require regular spacing. If this ever needs
    // to support irregular grids, use getRaw and resample at the call site instead.
    int64_t frequencyMs = ts.size() >= 2 ? ts[1] - ts[0] : endMs - startMs;
    if (frequencyMs <= 0) {
        throw std::invalid_argument("TimeSeriesService::get: frequency derived from timestamps must be positive.");
    }
    for (size_t i = 2; i < ts.size(); ++i) {
        if (ts[i] - ts[i - 1] != frequencyMs) {
            throw std::invalid_argument("TimeSeriesService::get: timestamps must be regularly spaced.");
        }
    }

    // Resolve via cache/repo/provider at the inferred frequency, then re-bind onto the
    // caller-owned timestamp vector so downstream callers can pointer-align.
    TimeSeries raw = get(id, startMs, endMs, frequencyMs);
    return raw.resampling(std::move(timestamps), InterpolationStrategy::Nearest);
}

TimeSeries TimeSeriesService::getRaw(const std::string& id, int64_t startMs, int64_t endMs) {
    // Try to find any local key that covers the range
    auto localKey = findLocalCoveringKey_(id, startMs, endMs, INT64_MAX);
    if (localKey) {
        return cache_->load(*localKey, startMs, endMs);
    }

    // No local coverage — fetch from provider at its native frequency
    if (!provider_) {
        throw std::runtime_error("No provider available and no local data for series: " + id);
    }

    auto caps = provider_->capabilities(id);
    SeriesKey nativeKey{id, caps.finestFrequencyMs};

    // Check partial coverage at the native key
    if (cache_->exists(nativeKey)) {
        auto cov = cache_->coverage(nativeKey);
        if (cov) {
            auto gaps = computeGaps(*cov, TimeRange{startMs, endMs});
            if (gaps.empty()) {
                return cache_->load(nativeKey, startMs, endMs);
            }
            fetchAndMergeGaps_(nativeKey, gaps);
            return cache_->load(nativeKey, startMs, endMs);
        }
    }

    // Full fetch
    TimeSeries fetched = stripNaN(provider_->load(id, startMs, endMs));
    if (fetched.size() == 0)
        throw std::runtime_error("TimeSeriesService::getRaw: no data returned by provider for series '" + id + "'");
    int64_t nowMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    CoverageInfo cov{nativeKey, fetched.getTimestamps().front(), fetched.getTimestamps().back(), "provider", nowMs};
    cache_->save(nativeKey, fetched, cov);
    return fetched;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::optional<SeriesKey> TimeSeriesService::findLocalCoveringKey_(const std::string& id, int64_t startMs, int64_t endMs,
                                                                  int64_t maxFrequencyMs) const {
    auto frequencies = cache_->availableFrequencies(id);
    // Sort ascending (finest / smallest ms value first)
    std::sort(frequencies.begin(), frequencies.end());

    for (int64_t freq : frequencies) {
        // Skip frequencies coarser than what we can derive from
        if (freq > maxFrequencyMs) continue;

        SeriesKey candidateKey{id, freq};
        auto cov = cache_->coverage(candidateKey);
        if (cov) {
            auto gaps = computeGaps(*cov, TimeRange{startMs, endMs});
            if (gaps.empty()) {
                return candidateKey;
            }
        }
    }
    return std::nullopt;
}

void TimeSeriesService::fetchAndMergeGaps_(const SeriesKey& key, const std::vector<TimeRange>& gaps) {
    if (!provider_) {
        throw std::runtime_error("No provider available to fetch gaps for series: " + key.SeriesId);
    }

    for (const auto& gap : gaps) {
        // Skip gaps narrower than the series frequency — a daily provider has no new
        // data points to fill within an intraday gap.
        if (gap.endTimeStampMs - gap.startTimeStampMs < key.frequencyInMs) continue;
        TimeSeries gapData = stripNaN(provider_->load(key.SeriesId, gap.startTimeStampMs, gap.endTimeStampMs));
        if (gapData.size() > 0) {
            // merge() recomputes coverage from the merged timestamps, so NaN-free data
            // automatically yields the correct coveredToMs for future gap detection.
            cache_->merge(key, gapData);
        }
    }
}
