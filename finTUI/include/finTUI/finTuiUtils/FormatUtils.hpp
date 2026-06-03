// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <string>

namespace finui::utils {

struct FormatUtils {
    // Generic number with thousands separator and 2 decimal places. e.g. "1,234.56"
    static std::string fmtNumber(double v);

    // Amount with currency code string appended. e.g. "1,234.56 USD", "1,235 JPY"
    static std::string currencyStringDisplay(double amount, const std::string& currencyCode);

    // Ratio (0.0–1.0) formatted as percentage. e.g. percentDisplay(0.1234) → "12.34%"
    static std::string percentDisplay(double ratio);

    FormatUtils() = delete;
};

}  // namespace finui::utils
