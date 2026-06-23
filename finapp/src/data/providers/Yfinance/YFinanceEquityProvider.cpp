// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finapp/data/providers/implementations/Yfinance/YFinanceEquityProvider.hpp"

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <string>

#include "finapp/common/Exception.hpp"
#include "finapp/common/logger/PrefixedLogger.hpp"
#include "finapp/data/providers/implementations/Yfinance/YfinanceUtils.hpp"
#include "finapp/finance/asset/Equity.hpp"
#include "finapp/finance/common/Currency.hpp"

namespace py = pybind11;

namespace finapp {

YFinanceEquityProvider::YFinanceEquityProvider(finapp::logging::ILogger* logger)
    : logger_{finapp::logging::PrefixedLogger::wrap(logger, "YFinanceEquityProvider")} {}

std::shared_ptr<finance::IAsset> YFinanceEquityProvider::fetch(const std::string& ticker) const {
    if (logger_) logger_->write(finapp::logging::Level::Info, "fetch '" + ticker + "'");
    PythonRuntime::pythonRuntime();
    py::gil_scoped_acquire gil;
    py::module_ yfinanceTool = py::module_::import("YFinanceFetcher");

    py::dict result = yfinanceTool.attr("fetch_equity_info")(ticker);

    std::string name = result["name"].cast<std::string>();
    std::string currency = result["currency"].cast<std::string>();
    std::string exchange = result["exchange"].cast<std::string>();
    std::string sector = result["sector"].cast<std::string>();

    // currencyFromString throws std::invalid_argument for unsupported currency codes.
    // Phase 1 supports: USD, EUR, JPY, KRW, CAD, GBP.
    finance::Currency denom = [&] {
        try {
            return finance::currencyFromString(currency);
        } catch (const std::invalid_argument& e) {
            throw finapp::InvalidArgument(std::string(e.what()) + " (ticker: " + ticker + ")");
        }
    }();

    if (logger_)
        logger_->write(finapp::logging::Level::Debug,
                       "fetch '" + ticker + "' → name='" + name + "' currency=" + currency + " exchange=" + exchange);
    return std::make_shared<finance::Equity>(ticker, name, denom, exchange, sector);
}

bool YFinanceEquityProvider::exists(const std::string& ticker) const {
    if (logger_) logger_->write(finapp::logging::Level::Debug, "exists '" + ticker + "'");
    PythonRuntime::pythonRuntime();
    py::gil_scoped_acquire gil;
    py::module_ yfinanceTool = py::module_::import("YFinanceFetcher");
    bool result = yfinanceTool.attr("equity_exists")(ticker).cast<bool>();
    if (logger_)
        logger_->write(finapp::logging::Level::Debug, "exists '" + ticker + "' → " + (result ? "true" : "false"));
    return result;
}

}  // namespace finapp
