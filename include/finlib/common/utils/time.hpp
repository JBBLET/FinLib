
#pragma once

#include <cstdint>
#include <string>

namespace common::utils::time {

    // Converts a Unix timestamp (seconds since epoch) to a formatted string
    std::string ts_to_date(std::int64_t timestamp);

}
