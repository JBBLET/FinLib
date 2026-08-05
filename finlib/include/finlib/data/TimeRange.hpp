// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/Format.hpp"
#include "finlib/data/CoverageInfo.hpp"

namespace ts {
struct TimeRange {
    Timestamp startTimeStampMs;
    Timestamp endTimeStampMs;
};

// Returns the sub-ranges of `requested` not already covered by `coverage`
std::vector<TimeRange> computeGaps(const CoverageInfo& coverage, const TimeRange& requested);
}  // namespace ts

// "[2024-07-01 .. 2026-07-01]". Gap computation reports in ranges, so this is what makes a
// coverage mismatch legible without hand-decoding epoch milliseconds.
template <>
struct std::formatter<ts::TimeRange> : std::formatter<std::string_view> {
    auto format(const ts::TimeRange& range, std::format_context& ctx) const -> std::format_context::iterator {
        const std::string rendered = std::format(
            "[{} .. {}]", ts::fmt::AsDate{range.startTimeStampMs}, ts::fmt::AsDate{range.endTimeStampMs});
        return std::formatter<std::string_view>::format(rendered, ctx);
    }
};
