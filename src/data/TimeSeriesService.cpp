// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finlib/data/services/TimeSeriesService.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "finlib/core/TimeSeries.hpp"
#include "finlib/data/CoverageInfo.hpp"
#include "finlib/data/SeriesKey.hpp"
#include "finlib/data/TimeRange.hpp"

TimeSeriesService::TimeSeriesService(std::shared_ptr<CachedTimeSeriesRepository> cache,
                                     std::shared_ptr<ITimeSeriesLoader> provider)
    : cache_(std::move(cache)), provider_(std::move(provider)) {}

// Public API

TimeSeries TimeSeriesService::get(const std::string& id, int64_t startMs, int64_t endMs, int64_t requestedFrequencyMs) {
    SeriesKey requestedKey{id, requestedFrequencyMs};

    // --- Step 1: exact key exists and covers the full range? ---
    if (cache_->exists(requestedKey)) {
        auto cov = cache_->coverage(requestedKey);
        if (cov) {
            auto gaps = computeGaps(*cov, TimeRange{startMs, endMs});
            if (gaps.empty()) {
                return cache_->load(requestedKey, startMs, endMs);
            }
            // Partial coverage — fetch gaps then serve
            fetchAndMergeGaps_(requestedKey, gaps);
            return cache_->load(requestedKey, startMs, endMs);
        }
    }

    // --- Step 2: finer frequency available locally that covers the range? ---
    auto localKey = findLocalCoveringKey_(id, startMs, endMs, requestedFrequencyMs);
    if (localKey) {
        TimeSeries finerData = cache_->load(*localKey, startMs, endMs);
        if (localKey->frequencyInMs < requestedFrequencyMs) {
            // Build a regular timestamp grid at requestedFrequencyMs and resample
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
                fetchAndMergeGaps_(requestedKey, gaps);
                return cache_->load(requestedKey, startMs, endMs);
            }
        }
    }

    // No existing data at all — full fetch
    TimeSeries fetched = provider_->load(id, startMs, endMs);

    int64_t nowMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();

    CoverageInfo cov{requestedKey, startMs, endMs, "provider", nowMs};

    cache_->save(requestedKey, fetched, cov);
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
        TimeSeries gapData = provider_->load(key.SeriesId, gap.startTimeStampMs, gap.endTimeStampMs);
        if (gapData.size() > 0) {
            cache_->merge(key, gapData);
        }
    }
}
