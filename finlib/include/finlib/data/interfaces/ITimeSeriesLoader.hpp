// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <string>
#include <vector>

#include "finlib/common/FinlibTypes.hpp"
#include "finlib/core/TimeSeries.hpp"

struct LoaderCapabilities {
    Timestamp earliestAvailableMS;
    // Finest frequency the provider can ever offer (used as fallback when tiers is empty).
    Timestamp finestFrequencyMs;

    // Range-to-frequency tiers, sorted by maxRangeMs ascending (narrowest first).
    // Each entry means: "for requested ranges <= maxRangeMs the provider delivers frequencyMs data."
    // If empty, finestFrequencyMs applies for all ranges.
    struct Tier {
        Timestamp maxRangeMs;
        Timestamp frequencyMs;
    };
    std::vector<Tier> tiers;

    // Returns the finest frequency this provider can deliver for the given range.
    Timestamp frequencyForRange(Timestamp rangeMs) const {
        for (const auto& tier : tiers) {
            if (rangeMs <= tier.maxRangeMs) return tier.frequencyMs;
        }
        return finestFrequencyMs;
    }
};

class ITimeSeriesLoader {
 public:
    ITimeSeriesLoader() = default;
    virtual ~ITimeSeriesLoader() = default;
    ITimeSeriesLoader(const ITimeSeriesLoader&) = default;
    ITimeSeriesLoader& operator=(const ITimeSeriesLoader&) = default;
    ITimeSeriesLoader(ITimeSeriesLoader&&) = default;
    ITimeSeriesLoader& operator=(ITimeSeriesLoader&&) = default;

    virtual TimeSeries load(const std::string& id, Timestamp startTimestampMs, Timestamp endTimestampMs) const = 0;
    virtual LoaderCapabilities capabilities(const std::string& id) const = 0;
};
