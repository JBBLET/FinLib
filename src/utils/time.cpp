#include "finlib/common/utils/time.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace common::utils::time {

    std::string ts_to_date(std::int64_t timestamp) {
        using namespace std::chrono;

        system_clock::time_point tp{milliseconds{timestamp}};
        std::time_t tt = system_clock::to_time_t(tp);

        std::tm tm{};
        gmtime_r(&tt, &tm);   
        char buf[11];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
        return buf;
    }

}
