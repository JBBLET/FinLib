// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finapp/providers/YFinanceProvider.hpp"

#include <cstdio>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "finlib/common/utils/TimeUtils.hpp"

using common::utils::time::msToStringDate;

LoaderCapabilities YFinanceProvider::capabilities(const std::string& /*id*/) const {
    // YFinance provides daily data; earliest is roughly 1970 but practically ~1990s
    constexpr int64_t dailyMs = 86'400'000;
    return LoaderCapabilities{0, dailyMs};
}

TimeSeries YFinanceProvider::load(const std::string& symbol, int64_t start_ts, int64_t end_ts) const {
    std::string start = msToStringDate(start_ts);
    std::string end = msToStringDate(end_ts);

    std::string cmd = python_ + " " + scriptPath_ + " \"" + symbol + "\" \"" + start + "\" \"" + end + "\" \"1d\"";

    std::cerr << "CMD: " << cmd << std::endl;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Failed to run yfinance loader");
    }

    std::vector<int64_t> timestamps;
    std::vector<double> values;

    char buffer[256];
    bool skip_header = true;

    while (fgets(buffer, sizeof(buffer), pipe)) {
        if (skip_header) {
            skip_header = false;
            continue;
        }

        int64_t ts;
        double val;
        if (sscanf(buffer, "%ld,%lf", &ts, &val) == 2) {
            timestamps.push_back(ts);
            values.push_back(val);
        }
    }

    pclose(pipe);

    return TimeSeries(symbol + "_" + start + "_" + end, std::move(timestamps), std::move(values));
}
