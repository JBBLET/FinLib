// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "finlib/common/FinlibTypes.hpp"

struct SeriesKey {
    std::string SeriesId;
    Timestamp frequencyInMs;
    bool operator==(const SeriesKey&) const = default;
};
template <>
struct std::hash<SeriesKey> {
    std::size_t operator()(const SeriesKey& key) const {
        std::size_t result = 12;
        result = result * 17 + std::hash<std::string>()(key.SeriesId);
        result = result * 17 + std::hash<int64_t>()(key.frequencyInMs);
        return result;
    }
};
