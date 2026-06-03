// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finTUI/finTuiUtils/FormatUtils.hpp"

#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
namespace finui::utils {

namespace {

std::string insertThousandsSep(std::string s) {
    const size_t start = (s[0] == '-') ? 1 : 0;
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

std::string FormatUtils::fmtNumber(double v) { return formatDouble(v, 2); }

std::string FormatUtils::currencyStringDisplay(double amount, const std::string& currencyCode) {
    const int decimals = (currencyCode == "JPY" || currencyCode == "KRW") ? 0 : 2;
    return formatDouble(amount, decimals) + " " + currencyCode;
}

std::string FormatUtils::percentDisplay(double ratio) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << (ratio * 100.0) << "%";
    return oss.str();
}

}  // namespace finui::utils
