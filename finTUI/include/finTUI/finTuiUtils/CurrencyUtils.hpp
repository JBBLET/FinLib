// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <string>

#include "portfolio.pb.h"

namespace finui::utils {

struct CurrencyUtils {
    // Amount with proto Currency enum appended. e.g. "1,234.56 USD", "1,235 JPY"

    static std::string currencyDisplay(double amount, finapp_rpc::Currency currency);
    static finapp_rpc::Currency currencyFromString(const std::string& Currency);
    CurrencyUtils() = delete;
};

}  // namespace finui::utils
