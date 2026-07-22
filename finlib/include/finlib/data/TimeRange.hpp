// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <vector>

#include "finlib/common/FinlibTypes.hpp"
#include "finlib/data/CoverageInfo.hpp"

namespace ts {
struct TimeRange {
    Timestamp startTimeStampMs;
    Timestamp endTimeStampMs;
};

// Returns the sub-ranges of `requested` not already covered by `coverage`
std::vector<TimeRange> computeGaps(const CoverageInfo& coverage, const TimeRange& requested);
}  // namespace ts
