// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finlib/data/TimeRange.hpp"

#include <vector>

#include "finlib/data/CoverageInfo.hpp"

namespace ts {

std::vector<TimeRange> computeGaps(const CoverageInfo& coverage, const TimeRange& requested) {
    std::vector<TimeRange> result;
    if (coverage.coveredFromMs > requested.startTimeStampMs) {
        result.push_back(TimeRange{requested.startTimeStampMs, coverage.coveredFromMs});
    }
    if (coverage.coveredToMs < requested.endTimeStampMs) {
        result.push_back(TimeRange{coverage.coveredToMs, requested.endTimeStampMs});
    }
    return result;
}

}  // namespace ts
