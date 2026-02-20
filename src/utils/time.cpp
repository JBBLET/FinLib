// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finlib/common/utils/time.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <stdexcept>

namespace common::utils::time {

int64_t parseIso8601ToMs(const std::string& input) {
    // Only accept YYYY-MM-DDTHH:MM:SS[.mmm](Z | ±HH:MM) format
    using namespace std::chrono;
    using std::stoi;

    // Split date-time and timezone
    auto pos = input.find_first_of("Z+-", 19);
    if (pos == std::string::npos)
        throw std::invalid_argument("Missing timezone information");

    std::string datetime = input.substr(0, pos);
    std::string tzPart   = input.substr(pos);

    // Parse base datetime
    int year, month, day, hour, min, sec;
    int millis = 0;

    if (datetime.size() < 19)
        throw std::invalid_argument("Invalid datetime format");

    year  = stoi(datetime.substr(0, 4));
    month = stoi(datetime.substr(5, 2));
    day   = stoi(datetime.substr(8, 2));
    hour  = stoi(datetime.substr(11, 2));
    min   = stoi(datetime.substr(14, 2));
    sec   = stoi(datetime.substr(17, 2));

    // Optional milliseconds
    if (datetime.size() > 19 && datetime[19] == '.') {
        millis = stoi(datetime.substr(20, 3));
    }

    std::chrono::sys_days days = std::chrono::year {year}
        / std::chrono::month {static_cast<unsigned>(month)}
        / std::chrono::day {static_cast<unsigned>(day)};

    std::chrono::sys_time<milliseconds> tp = days
        + std::chrono::hours {hour}
        + std::chrono::minutes {min}
        + std::chrono::seconds {sec}
        + std::chrono::milliseconds {millis};

    // Apply timezone offset
    if (tzPart == "Z") {
    } else {
        int sign = (tzPart[0] == '+') ? 1 : -1;
        int tzHour = stoi(tzPart.substr(1, 2));
        int tzMin  = stoi(tzPart.substr(4, 2));

        auto offset = std::chrono::hours{tzHour} + std::chrono::minutes{tzMin};
        tp -= sign * offset;
    }

    return tp.time_since_epoch().count();
}

std::string msToStringISO8601(int64_t ms) {
    using std::chrono::sys_time;
    using std::chrono::duration_cast;
    using std::chrono::floor;
    using std::chrono::seconds;
    using std::chrono::milliseconds;

    sys_time<milliseconds> tp{milliseconds{ms}};

    auto sec_tp = floor<seconds>(tp);
    auto ms_part = duration_cast<milliseconds>(tp - sec_tp).count();

    return std::format("{:%Y-%m-%dT%H:%M:%S}.{:03}Z",
                       sec_tp,
                       ms_part);
}

std::string msToStringDate(int64_t ms) {
    using namespace std::chrono;

    sys_time<milliseconds> tp{milliseconds{ms}};

    auto days = floor<std::chrono::days>(tp);
    std::chrono::year_month_day ymd{days};

    return std::format("{:%Y-%m-%d}", ymd);
}
}  // namespace common::utils::time
