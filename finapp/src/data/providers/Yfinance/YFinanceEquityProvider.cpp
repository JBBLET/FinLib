// "Copyright (c) 2026 JBBLET All Rights Reserved."
#include "finapp/data/providers/implementations/Yfinance/YFinanceEquityProvider.hpp"

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <string>

#include "finapp/data/providers/implementations/Yfinance/YfinanceUtils.hpp"
#include "finapp/finance/asset/Equity.hpp"
#include "finapp/finance/common/Currency.hpp"

namespace py = pybind11;

namespace finapp {

using namespace finance;

std::shared_ptr<finance::IAsset> YFinanceEquityProvider::fetch(const std::string& ticker) const {
    PythonRuntime::pythonRuntime();
    py::module_ yfinanceTool = py::module_::import("YFinanceFetcher");

    py::dict result = yfinanceTool.attr("fetch_equity_info")(ticker);

    std::string name     = result["name"].cast<std::string>();
    std::string currency = result["currency"].cast<std::string>();
    std::string exchange = result["exchange"].cast<std::string>();
    std::string sector   = result["sector"].cast<std::string>();

    // currencyFromString throws std::out_of_range for unsupported currency codes.
    // Phase 1 supports: USD, EUR, JPY, KRW, CAD, GBP.
    Currency denom = currencyFromString(currency);

    return std::make_shared<Equity>(ticker, name, denom, exchange, sector);
}

bool YFinanceEquityProvider::exists(const std::string& ticker) const {
    PythonRuntime::pythonRuntime();
    py::module_ yfinanceTool = py::module_::import("YFinanceFetcher");
    return yfinanceTool.attr("equity_exists")(ticker).cast<bool>();
}

}  // namespace finapp
