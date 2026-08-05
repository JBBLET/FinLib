// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <format>
#include <string>
#include <string_view>

#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/Format.hpp"
#include "finlib/data/SeriesKey.hpp"

namespace ts {

struct CoverageInfo {
    SeriesKey key;
    Timestamp coveredFromMs;
    Timestamp coveredToMs;
    std::string source;
    Timestamp lastUpdatedMs;
};
}  // namespace ts

// What the cache believes it holds. Printed whole because a coverage bug is almost always a
// disagreement between the key, the range, and how stale the entry is.
template <>
struct std::formatter<ts::CoverageInfo> : std::formatter<std::string_view> {
    auto format(const ts::CoverageInfo& coverage, std::format_context& ctx) const -> std::format_context::iterator {
        const std::string rendered = std::format("Coverage[{}, {} .. {}, source='{}', updated {}]",
                                                 coverage.key,
                                                 ts::fmt::AsDate{coverage.coveredFromMs},
                                                 ts::fmt::AsDate{coverage.coveredToMs},
                                                 coverage.source,
                                                 ts::fmt::AsDateTime{coverage.lastUpdatedMs});
        return std::formatter<std::string_view>::format(rendered, ctx);
    }
};
