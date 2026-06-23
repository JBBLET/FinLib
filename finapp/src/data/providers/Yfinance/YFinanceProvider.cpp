// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finapp/data/providers/implementations/Yfinance/YFinanceProvider.hpp"

#include <pybind11/embed.h>  // everything needed for embedding
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "finapp/common/logger/PrefixedLogger.hpp"
#include "finapp/data/providers/implementations/Yfinance/YfinanceUtils.hpp"
#include "finlib/common/utils/TimeUtils.hpp"
#include "finlib/core/TimeSeries.hpp"

using common::utils::time::msToStringDate;

namespace py = pybind11;

namespace finapp {

YFinanceProvider::YFinanceProvider(std::string pythonExec, std::string scriptPath, finapp::logging::ILogger* logger)
    : python_(std::move(pythonExec)),
      scriptPath_(std::move(scriptPath)),
      logger_{finapp::logging::PrefixedLogger::wrap(logger, "YFinanceProvider")} {}

LoaderCapabilities YFinanceProvider::capabilities(const std::string& /*id*/) const {
    constexpr int64_t kDayMs = 86'400'000LL;
    // Tiers sorted by maxRangeMs ascending; the last entry catches all longer ranges.
    return LoaderCapabilities{
        0LL,       // earliestAvailableMS (~1970 for daily, rolling window for intraday)
        60'000LL,  // finestFrequencyMs: 1 minute (fallback if tiers is empty)
        {
            {7 * kDayMs, 60'000LL},                               // ≤7d  → 1m
            {60 * kDayMs, 300'000LL},                             // ≤60d → 5m
            {std::numeric_limits<int64_t>::max(), 86'400'000LL},  // unlimited → 1d
        },
    };
}

TimeSeries YFinanceProvider::load(const std::string& symbol, int64_t start_ts, int64_t end_ts,
                                   std::optional<Timestamp> requestedFrequency) const {
    constexpr int64_t kDayMs = 86'400'000LL;
    // Use explicitly requested frequency when provided; otherwise auto-detect from range.
    const Timestamp freqMs = requestedFrequency.value_or(capabilities(symbol).frequencyForRange(end_ts - start_ts));
    std::string interval;
    if (freqMs <= 60'000LL)
        interval = "1m";
    else if (freqMs <= 300'000LL)
        interval = "5m";
    else
        interval = "1d";

    if (logger_)
        logger_->write(finapp::logging::Level::Info,
                       "load '" + symbol + "' [" + msToStringDate(start_ts) + ".." + msToStringDate(end_ts) +
                           "] interval=" + interval);

    PythonRuntime::pythonRuntime();
    py::gil_scoped_acquire gil;
    py::module_ yfinanceTool = py::module_::import("YFinanceFetcher");

    std::string start = msToStringDate(start_ts);
    // yfinance end is exclusive — advance by one day so end_ts's calendar day is included.
    std::string end = msToStringDate(end_ts + kDayMs);

    py::dict result = yfinanceTool.attr("fetch_ohlcv")(symbol, start, end, interval);
    auto timestamps = result["timestamps_ms"].cast<std::vector<int64_t>>();
    // "close" is split- and dividend-adjusted (yfinance auto_adjust=True default).
    auto closes = result["close"].cast<std::vector<double>>();
    if (logger_)
        logger_->write(finapp::logging::Level::Info,
                       "load '" + symbol + "' complete — " + std::to_string(closes.size()) + " bars");
    return TimeSeries("Test", timestamps, closes);
}

}  // namespace finapp
