// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finapp/data/providers/implementations/Yfinance/YFinanceProvider.hpp"

#include <pybind11/embed.h>  // everything needed for embedding
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <string>
#include <vector>

#include "finapp/data/providers/implementations/Yfinance/YfinanceUtils.hpp"
#include "finlib/common/utils/TimeUtils.hpp"
#include "finlib/core/TimeSeries.hpp"

using common::utils::time::msToStringDate;

namespace py = pybind11;

namespace finapp {

LoaderCapabilities YFinanceProvider::capabilities(const std::string& id) const {
    // YFinance provides daily data; earliest is roughly 1970 but practically ~1990s
    // TODO(Change to support intraday for less than 6 Days)
    constexpr int64_t dailyMs = 86'400'000;
    return LoaderCapabilities{0, dailyMs};
}

TimeSeries YFinanceProvider::load(const std::string& symbol, int64_t start_ts, int64_t end_ts) const {
    PythonRuntime::pythonRuntime();
    py::gil_scoped_acquire gil;

    py::module_ yfinanceTool = py::module_::import("YFinanceFetcher");
    std::string start = msToStringDate(start_ts);
    // yfinance end is exclusive — advance by one day so end_ts's calendar day is included.
    std::string end = msToStringDate(end_ts + 86'400'000LL);

    py::dict result = yfinanceTool.attr("fetch_ohlcv")(symbol, start, end, "1d");
    auto timestamps = result["timestamps_ms"].cast<std::vector<int64_t>>();
    // "close" is split- and dividend-adjusted (yfinance auto_adjust=True default).
    auto closes = result["close"].cast<std::vector<double>>();
    // std::string cmd = python_ + " " + scriptPath_ + " \"" + symbol + "\" \"" + start + "\" \"" +
    // end + "\" \"1d\"";

    // std::cerr << "CMD: " << cmd << std::endl;

    // FILE* pipe = popen(cmd.c_str(), "r");
    // intf (!pipe) {
    //     throw std::runtime_error("Failed to run yfinance loader");
    // }

    /// std::vector<int64_t> timestamps;
    // std::vector<double> values;

    // char buffer[256];
    // bool skip_header = true;

    // while (fgets(buffer, sizeof(buffer), pipe)) {
    //     if (skip_header) {
    //         skip_header = false;
    //         continue;
    //     }

    //    int64_t ts;
    //    double val;
    //    if (sscanf(buffer, "%ld,%lf", &ts, &val) == 2) {
    //        timestamps.push_back(ts);
    //        values.push_back(val);
    //    }
    //}

    // pclose(pipe);
    return TimeSeries("Test", timestamps, closes);
}

}  // namespace finapp
