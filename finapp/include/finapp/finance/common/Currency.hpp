// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace finance {
enum class Currency : uint8_t { USD, EUR, JPY, KRW, CAD, GBP };

static std::unordered_map<std::string, Currency> stringToCurrencyMap = {{"USD", Currency::USD}, {"EUR", Currency::EUR},
                                                                        {"JPY", Currency::JPY}, {"KRW", Currency::KRW},
                                                                        {"CAD", Currency::CAD}, {"GBP", Currency::GBP}};

inline const std::string toString(Currency c) {
    switch (c) {
        case (Currency::USD):
            return "USD";
        case (Currency::EUR):
            return "EUR";
        case (Currency::JPY):
            return "JPY";
        case (Currency::KRW):
            return "KRW";
        case (Currency::CAD):
            return "CAD";
        case (Currency::GBP):
            return "GBP";
    }
}
inline Currency currencyFromString(const std::string& id) {
    std::string upperCaseStr = id;
    std::transform(upperCaseStr.begin(), upperCaseStr.end(), upperCaseStr.begin(), ::toupper);
    Currency currency = stringToCurrencyMap.at(upperCaseStr);
    return currency;
}
}  // namespace finance
