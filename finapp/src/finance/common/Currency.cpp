// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/finance/common/Currency.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>

#include "finapp/common/Error.hpp"

namespace finance {

namespace {
const std::unordered_map<std::string, Currency> stringToCurrencyMap = {{"USD", Currency::USD},
                                                                       {"EUR", Currency::EUR},
                                                                       {"JPY", Currency::JPY},
                                                                       {"KRW", Currency::KRW},
                                                                       {"CAD", Currency::CAD},
                                                                       {"GBP", Currency::GBP}};
}  // namespace

const std::string toString(Currency c) {
    switch (c) {
        case Currency::USD:
            return "USD";
        case Currency::EUR:
            return "EUR";
        case Currency::JPY:
            return "JPY";
        case Currency::KRW:
            return "KRW";
        case Currency::CAD:
            return "CAD";
        case Currency::GBP:
            return "GBP";
        default:
            return "";
    }
}

Currency currencyFromString(const std::string& id) {
    std::string upperCaseStr = id;
    std::transform(upperCaseStr.begin(), upperCaseStr.end(), upperCaseStr.begin(), ::toupper);
    auto it = stringToCurrencyMap.find(upperCaseStr);
    ensure<InvalidArgument>(it != stringToCurrencyMap.end(),
                            "Unsupported currency code: '{}'. Supported: USD, EUR, JPY, KRW, CAD, GBP.", id);
    return it->second;
}

}  // namespace finance
