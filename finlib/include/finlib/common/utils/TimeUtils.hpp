// "Copyright (c) 2026 JBBLET All Rights Reserved."
#pragma once

#include <cstdint>
#include <string>

#include "finlib/common/FinlibTypes.hpp"

namespace common::utils::time {
// Converts a Unix timestamp (seconds since epoch) to a formatted string
std::string msToStringISO8601(Timestamp ms);
int64_t parseIso8601ToMs(const std::string& input);
std::string msToStringDate(Timestamp ms);
}  // namespace common::utils::time
