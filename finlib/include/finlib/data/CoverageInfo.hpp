// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <string>

#include "finlib/common/FinlibTypes.hpp"
#include "finlib/data/SeriesKey.hpp"

struct CoverageInfo {
    SeriesKey key;
    Timestamp coveredFromMs;
    Timestamp coveredToMs;
    std::string source;
    Timestamp lastUpdatedMs;
};
