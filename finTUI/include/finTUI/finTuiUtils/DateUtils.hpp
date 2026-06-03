// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <string>

namespace finui::utils {

struct DateUtils {
    static std::string fmtDate(int64_t ms);
    static std::string todayDateString();
    static int64_t parseDate(const std::string& s);
    DateUtils() = delete;
};

}  // namespace finui::utils
