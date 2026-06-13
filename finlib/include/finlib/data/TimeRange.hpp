// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <cstdint>
#include <vector>

#include "finlib/common/FinlibTypes.hpp"
#include "finlib/data/CoverageInfo.hpp"

struct TimeRange {
    Timestamp startTimeStampMs;
    Timestamp endTimeStampMs;
};

inline std::vector<TimeRange> computeGaps(const CoverageInfo& coverage, const TimeRange& requested) {
    std::vector<TimeRange> result;
    if (coverage.coveredFromMs > requested.startTimeStampMs) {
        result.push_back(TimeRange{requested.startTimeStampMs, coverage.coveredFromMs});
    }
    if (coverage.coveredToMs < requested.endTimeStampMs) {
        result.push_back(TimeRange{coverage.coveredToMs, requested.endTimeStampMs});
    }
    return result;
}
