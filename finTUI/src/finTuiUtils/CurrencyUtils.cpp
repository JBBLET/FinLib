// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/finTuiUtils/CurrencyUtils.hpp"

#include <exception>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

#include "portfolio.pb.h"

namespace finui::utils {

namespace {

std::string insertThousandsSep(std::string s) {
    const size_t start = (!s.empty() && s[0] == '-') ? 1 : 0;
    int pos = static_cast<int>(s.size()) - 3;
    while (pos > static_cast<int>(start)) {
        s.insert(static_cast<size_t>(pos), ",");
        pos -= 3;
    }
    return s;
}

std::string formatDouble(double v, int decimals) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << v;
    std::string s = oss.str();
    const size_t dotPos = s.find('.');
    std::string intPart = (dotPos == std::string::npos) ? s : s.substr(0, dotPos);
    std::string fracPart = (dotPos == std::string::npos) ? "" : s.substr(dotPos);
    return insertThousandsSep(std::move(intPart)) + fracPart;
}

}  // namespace

std::string CurrencyUtils::currencyDisplay(double amount, finapp_rpc::Currency currency) {
    static constexpr const char* kCodes[] = {"USD", "EUR", "JPY", "KRW", "CAD", "GBP"};
    const int idx = static_cast<int>(currency);
    const char* code = (idx >= 0 && idx < 6) ? kCodes[idx] : "???";
    const int decimals = (currency == finapp_rpc::JPY || currency == finapp_rpc::KRW) ? 0 : 2;
    return formatDouble(amount, decimals) + " " + code;
}
finapp_rpc::Currency CurrencyUtils::currencyFromString(const std::string& currency) {
    finapp_rpc::Currency currencyRPC;
    if (finapp_rpc::Currency_Parse(currency, &currencyRPC)) {
        return currencyRPC;
    } else {
        throw(std::runtime_error("Currency string not found"));
    }
}
}  // namespace finui::utils
