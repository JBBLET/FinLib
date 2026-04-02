// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <cstdint>
#include <string>

#include "finlib/data/SeriesKey.hpp"

struct CoverageInfo {
    SeriesKey key;
    int64_t coveredFromMs;
    int64_t coveredToMs;
    std::string source;
    int64_t lastUpdatedMs;
};
