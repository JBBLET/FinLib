// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <format>
#include <functional>
#include <string>
#include <string_view>

#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/Format.hpp"

namespace ts {

struct SeriesKey {
    std::string SeriesId;
    Timestamp frequencyInMs;
    bool operator==(const SeriesKey&) const = default;
};
}  // namespace ts

// "AAPL@1d". This is the identity that repository and cache diagnostics key on, and the
// pair was previously spelled out as `'{}' freq={}ms` at every one of those call sites.
template <>
struct std::formatter<ts::SeriesKey> : std::formatter<std::string_view> {
    auto format(const ts::SeriesKey& key, std::format_context& ctx) const -> std::format_context::iterator {
        const std::string rendered = std::format("{}@{}", key.SeriesId, ts::fmt::formatDuration(key.frequencyInMs));
        return std::formatter<std::string_view>::format(rendered, ctx);
    }
};
template <>
struct std::hash<ts::SeriesKey> {
    std::size_t operator()(const ts::SeriesKey& key) const {
        std::size_t result = 12;
        result = result * 17 + std::hash<std::string>()(key.SeriesId);
        result = result * 17 + std::hash<ts::Timestamp>()(key.frequencyInMs);
        return result;
    }
};
