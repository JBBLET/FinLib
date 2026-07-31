// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finapp/data/providers/implementations/Yfinance/YFinanceProvider.hpp"

#include <pybind11/embed.h>  // everything needed for embedding
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "finapp/common/Log.hpp"
#include "finapp/data/providers/implementations/Yfinance/YfinanceUtils.hpp"
#include "finlib/common/utils/TimeUtils.hpp"

using ts::common::utils::time::msToStringDate;

namespace py = pybind11;

namespace finapp {

YFinanceProvider::YFinanceProvider() = default;

LoaderCapabilities YFinanceProvider::capabilities(const std::string& /*id*/) const {
    constexpr int64_t kDayMs = 86'400'000LL;
    constexpr int64_t kIntradayKey = 60'000LL;
    return LoaderCapabilities{
        0LL,           // earliestAvailableMS (~1970 for daily, rolling window for intraday)
        kIntradayKey,  // finestFrequencyMs
        {
            {60 * kDayMs, kIntradayKey},                    // ≤60d  → intraday bucket
            {std::numeric_limits<int64_t>::max(), kDayMs},  // beyond → daily bucket
        },
    };
}

TimeSeries YFinanceProvider::load(const std::string& symbol, int64_t start_ts, int64_t end_ts,
                                  std::optional<Timestamp> requestedFrequency) const {
    constexpr int64_t kDayMs = 86'400'000LL;
    const int64_t rangeMs = end_ts - start_ts;
    const Timestamp freqMs = requestedFrequency.value_or(capabilities(symbol).frequencyForRange(rangeMs));
    std::string interval;
    if (freqMs >= kDayMs)
        interval = "1d";
    else if (rangeMs <= 7 * kDayMs)
        interval = "1m";
    else
        interval = "5m";

    logging::info("load '{}' [{}..{}] interval={}", symbol, msToStringDate(start_ts), msToStringDate(end_ts),
                  interval);

    PythonRuntime::pythonRuntime();
    py::gil_scoped_acquire gil;
    py::module_ yfinanceTool = py::module_::import("YFinanceFetcher");

    std::string start = msToStringDate(start_ts);
    std::string end = msToStringDate(end_ts + kDayMs);

    py::dict result = yfinanceTool.attr("fetch_ohlcv")(symbol, start, end, interval);
    auto timestamps = result["timestamps_ms"].cast<std::vector<int64_t>>();
    auto closes = result["close"].cast<std::vector<double>>();
    logging::info("load '{}' complete — {} bars", symbol, closes.size());
    return TimeSeries("Test", timestamps, closes);
}

}  // namespace finapp
