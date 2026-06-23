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

TimeSeries TimeSeriesService::get(const std::string& id, Timestamp startMs, Timestamp endMs,
                                  Timestamp requestedFrequencyMs) {
    SeriesKey requestedKey{id, requestedFrequencyMs};

    // --- Step 1: exact key exists and covers the full range? ---
    if (cache_->exists(requestedKey)) {
        auto cov = cache_->coverage(requestedKey);
        if (cov) {
            auto gaps = computeGaps(*cov, TimeRange{startMs, endMs});
            if (gaps.empty()) {
                if (logger_)
                    logger_->write(logging::Level::Debug,
                                   "get '" + id + "' freq=" + std::to_string(requestedFrequencyMs) + "ms: cache hit");
                return cache_->load(requestedKey, startMs, endMs);
            }
            // Before going to the provider, check if a strictly-finer local key covers
            // the full range.  If yes, fall through to Step 2: resample synthetically
            // for this call without saving (interpolated values must not be persisted).
            if (findLocalCoveringKey_(id, startMs, endMs, requestedFrequencyMs - 1)) {
                if (logger_)
                    logger_->write(logging::Level::Debug,
                                   "get '" + id + "' freq=" + std::to_string(requestedFrequencyMs) +
                                       "ms: partial cache but finer local covers range — synthetic resample");
            } else {
                if (logger_)
                    logger_->write(logging::Level::Debug,
                                   "get '" + id + "' freq=" + std::to_string(requestedFrequencyMs) +
                                       "ms: partial cache, filling " + std::to_string(gaps.size()) + " gap(s)");
                fetchAndMergeGaps_(requestedKey, gaps);
                return cache_->load(requestedKey, startMs, endMs);
            }
        }
    }

    // --- Step 2: any local key (finer or coarser) that covers the full range? ---
    // Coarser data is resampled to the requested grid via nearest-neighbour, which gives
    // correct fill-forward behaviour for weekends and holidays.
    auto localKey = findLocalCoveringKey_(id, startMs, endMs, INT64_MAX);
    if (localKey) {
        if (logger_)
            logger_->write(logging::Level::Debug,
                           "get '" + id + "' freq=" + std::to_string(requestedFrequencyMs) +
                               "ms: local resample from freq=" + std::to_string(localKey->frequencyInMs) + "ms");
        TimeSeries localData = cache_->load(*localKey, startMs, endMs);
        if (localKey->frequencyInMs != requestedFrequencyMs) {
            Timestamps targetTimestamps;
            for (Timestamp t = startMs; t <= endMs; t += requestedFrequencyMs) {
                targetTimestamps.push_back(t);
            }
            return localData.resampling(targetTimestamps, InterpolationStrategy::Nearest);
        }
        return localData;
    }

    // --- Step 3: fetch from provider ---
    if (!provider_) {
        throw std::runtime_error("No provider available and no local data for series: " + id);
    }

    // Ask the provider what frequency it can deliver for this specific range, then use
    // that as the cache key.  This keeps TimeSeriesService provider-agnostic: the service
    // drives all logic from declared capabilities, not from provider internals.
    const auto caps = provider_->capabilities(id);
    const Timestamp providerFreqMs = caps.frequencyForRange(endMs - startMs);
    const SeriesKey fetchKey{id, providerFreqMs};

    // Check partial coverage at the fetch key — fill only the gaps.
    if (cache_->exists(fetchKey)) {
        auto cov = cache_->coverage(fetchKey);
        if (cov) {
            auto gaps = computeGaps(*cov, TimeRange{startMs, endMs});
            if (!gaps.empty()) {
                if (logger_)
                    logger_->write(logging::Level::Info,
                                   "get '" + id + "' freq=" + std::to_string(requestedFrequencyMs) +
                                       "ms: provider gap fill " + std::to_string(gaps.size()) + " gap(s)");
                fetchAndMergeGaps_(fetchKey, gaps);
            }
            TimeSeries raw = cache_->load(fetchKey, startMs, endMs);
            if (providerFreqMs != requestedFrequencyMs) {
                Timestamps grid;
                for (Timestamp t = startMs; t <= endMs; t += requestedFrequencyMs) grid.push_back(t);
                return raw.resampling(grid, InterpolationStrategy::Nearest);
            }
            return raw;
        }
    }

    // No existing data at all — full fetch.
    if (logger_)
        logger_->write(logging::Level::Info,
                       "get '" + id + "' freq=" + std::to_string(requestedFrequencyMs) + "ms: provider full fetch [" +
                           std::to_string(startMs) + ", " + std::to_string(endMs) + "]");
    TimeSeries fetched = stripNaN(provider_->load(id, startMs, endMs));
    if (fetched.size() == 0) {
        throw std::runtime_error("TimeSeriesService::get: no data returned by provider for series '" + id +
                                 "' (ticker may be delisted or unavailable)");
    }

    Timestamp nowMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    CoverageInfo cov{fetchKey, fetched.getTimestamps().front(), fetched.getTimestamps().back(), "provider", nowMs};
    cache_->save(fetchKey, fetched, cov);

    if (providerFreqMs != requestedFrequencyMs) {
        Timestamps grid;
        for (Timestamp t = startMs; t <= endMs; t += requestedFrequencyMs) grid.push_back(t);
        return fetched.resampling(grid, InterpolationStrategy::Nearest);
    }
    return fetched;
}

TimeSeries TimeSeriesService::getResampled(const std::string& id, Timestamp startMs, Timestamp endMs,
                                           Timestamp requestedFrequencyMs, InterpolationStrategy strategy) {
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

        if (auto localKey = findLocalCoveringKey_(id, startMs, endMs, INT64_MAX)) {
            return *localKey;
        }

        if (!provider_) throw std::runtime_error("No provider available and no local data for series: " + id);

        const auto caps = provider_->capabilities(id);
        const Timestamp providerFreqMs = caps.frequencyForRange(endMs - startMs);
        const SeriesKey fetchKey{id, providerFreqMs};

        if (cache_->exists(fetchKey)) {
            auto cov = cache_->coverage(fetchKey);
            if (cov) {
                auto gaps = computeGaps(*cov, TimeRange{startMs, endMs});
                if (!gaps.empty()) fetchAndMergeGaps_(fetchKey, gaps);
                return fetchKey;
            }
        }

        TimeSeries fetched = stripNaN(provider_->load(id, startMs, endMs));
        if (fetched.size() == 0)
            throw std::runtime_error("TimeSeriesService::getResampled: no data returned by provider for series '" + id +
                                     "' (ticker may be delisted or unavailable)");

        int64_t nowMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        CoverageInfo cov{fetchKey, fetched.getTimestamps().front(), fetched.getTimestamps().back(), "provider", nowMs};
        cache_->save(fetchKey, fetched, cov);
        return fetchKey;
    };

    const SeriesKey resolvedKey = ensureAndResolveKey();

    // Load full NaN-free series and resample to a regular grid with the requested strategy.
    // This fills weekends, holidays, and any tail beyond the last published data point.
    TimeSeries full = cache_->load(resolvedKey);
    Timestamps grid;
    for (Timestamp t = startMs; t <= endMs; t += requestedFrequencyMs) grid.push_back(t);
    if (grid.empty()) grid.push_back(startMs);
    return full.resampling(grid, strategy);
}

TimeSeries TimeSeriesService::get(const std::string& id, TimestampsPtr timestamps) {
    if (!timestamps || timestamps->empty()) {
        throw std::invalid_argument("TimeSeriesService::get: timestamps must be non-empty.");
    }
    const auto& ts = *timestamps;
    if (!std::is_sorted(ts.begin(), ts.end())) {
        throw std::invalid_argument("TimeSeriesService::get: timestamps must be sorted.");
    }

    Timestamp startMs = ts.front();
    Timestamp endMs = ts.back();

    // Infer frequency from the grid and require regular spacing. If this ever needs
    // to support irregular grids, use getRaw and resample at the call site instead.
    Timestamp frequencyMs = ts.size() >= 2 ? ts[1] - ts[0] : endMs - startMs;
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

TimeSeries TimeSeriesService::getRaw(const std::string& id, Timestamp startMs, Timestamp endMs) {
    // Try to find any local key that covers the range
    auto localKey = findLocalCoveringKey_(id, startMs, endMs, INT64_MAX);
    if (localKey) {
        return cache_->load(*localKey, startMs, endMs);
    }

    // No local coverage — fetch from provider at its native frequency
    if (!provider_) {
        throw std::runtime_error("No provider available and no local data for series: " + id);
    }

    const auto caps = provider_->capabilities(id);
    const Timestamp nativeFreqMs = caps.frequencyForRange(endMs - startMs);
    SeriesKey nativeKey{id, nativeFreqMs};

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

double TimeSeriesService::getSinglePoint(const std::string& id, Timestamp ts) {
    // Step 1: any cached key whose coverage spans ts — check for an exact non-NaN match.
    auto localKey = findLocalCoveringKey_(id, ts, ts, INT64_MAX);
    if (localKey) {
        TimeSeries series = cache_->load(*localKey);
        auto exact = series.exactValue(ts);
        if (exact.has_value() && !std::isnan(*exact)) {
            if (logger_)
                logger_->write(logging::Level::Debug,
                               "getSinglePoint '" + id + "' ts=" + std::to_string(ts) + ": cache exact hit");
            return *exact;
        }
    }

    // Step 2: no exact hit in cache — fetch a window from the provider.
    if (!provider_) {
        throw std::runtime_error("TimeSeriesService::getSinglePoint: no provider for series '" + id + "'");
    }

    const auto caps = provider_->capabilities(id);

    // The window must be wide enough that load(ts-w, ts+w) triggers the right tier for
    // the age of ts. Providers like YFinance only serve 1m for the last 7 days and 5m
    // for the last 60 days — a fixed narrow window always lands in the finest tier and
    // fails for historical timestamps.
    int64_t nowMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    Timestamp ageMs = (ts < nowMs) ? (nowMs - ts) : 0LL;
    Timestamp freqMs = caps.frequencyForRange(ageMs);
    // 2*windowMs must exceed the previous tier boundary so load() lands in the right tier.
    Timestamp minRange = caps.minFetchRangeFor(ageMs);
    Timestamp windowMs = std::max((minRange / 2) + 1, 5 * freqMs);

    TimeSeries raw = provider_->load(id, ts - windowMs, ts + windowMs);
    if (raw.size() == 0) {
        // Provider returned nothing — market may be closed (weekend/holiday).
        // Fall back to the most recent cached bar at or before ts (look-back, no look-ahead).
        auto freqs = cache_->availableFrequencies(id);
        std::sort(freqs.begin(), freqs.end());
        for (Timestamp freq : freqs) {
            SeriesKey k{id, freq};
            auto cov = cache_->coverage(k);
            if (!cov || cov->coveredFromMs > ts) continue;
            try {
                double v = cache_->load(k).latestValue(ts);
                if (!std::isnan(v)) {
                    if (logger_)
                        logger_->write(logging::Level::Debug,
                                       "getSinglePoint '" + id + "' ts=" + std::to_string(ts) +
                                           ": provider empty, cache look-back");
                    return v;
                }
            } catch (...) {}
        }
        throw std::runtime_error("TimeSeriesService::getSinglePoint: provider returned no data for '" + id + "'");
    }

    // Check the exact point on raw data before stripping NaN.
    auto providerExact = raw.exactValue(ts);
    TimeSeries clean = stripNaN(std::move(raw));

    if (clean.size() > 0) {
        SeriesKey key{id, freqMs};
        CoverageInfo cov{key, clean.getTimestamps().front(), clean.getTimestamps().back(), "provider", nowMs};
        cache_->save(key, clean, cov);
    }

    // Exact non-NaN from provider — done.
    if (providerExact.has_value() && !std::isnan(*providerExact)) {
        if (logger_)
            logger_->write(logging::Level::Debug,
                           "getSinglePoint '" + id + "' ts=" + std::to_string(ts) + ": provider exact hit");
        return *providerExact;
    }

    // Provider had NaN or no point at ts — look back on clean data (no look-ahead).
    if (logger_)
        logger_->write(logging::Level::Debug,
                       "getSinglePoint '" + id + "' ts=" + std::to_string(ts) + ": provider NaN, look-back");
    if (clean.size() == 0) {
        throw std::runtime_error("TimeSeriesService::getSinglePoint: all provider data is NaN for '" + id + "'");
    }
    return clean.latestValue(ts);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::optional<SeriesKey> TimeSeriesService::findLocalCoveringKey_(const std::string& id, Timestamp startMs,
                                                                  Timestamp endMs, Timestamp maxFrequencyMs) const {
    auto frequencies = cache_->availableFrequencies(id);
    // Sort ascending (finest / smallest ms value first)
    std::sort(frequencies.begin(), frequencies.end());

    for (Timestamp freq : frequencies) {
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
        TimeSeries gapData =
            stripNaN(provider_->load(key.SeriesId, gap.startTimeStampMs, gap.endTimeStampMs, key.frequencyInMs));
        if (gapData.size() > 0) {
            cache_->merge(key, gapData);
        }
    }
}
