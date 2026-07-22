// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace finance {
enum class Currency : uint8_t { USD, EUR, JPY, KRW, CAD, GBP };

inline const std::vector<Currency> valid_currencies = {
    Currency::USD, Currency::EUR, Currency::JPY, Currency::KRW, Currency::CAD, Currency::GBP};

// Canonical ISO code for a currency ("" for an unknown enumerator).
const std::string toString(Currency c);

// Parses a currency code (case-insensitive). Throws finapp::InvalidArgument on
// an unsupported code.
Currency currencyFromString(const std::string& id);
}  // namespace finance

template <>
struct std::hash<finance::Currency> {
    std::uint8_t operator()(const finance::Currency& currency) const { return static_cast<uint8_t>(currency); }
};

template <typename T>
static std::unordered_map<const finance::Currency, T> currencyMapInitialization(const T& initValue) {
    std::unordered_map<finance::Currency, T> output;
    output.reserve(finance::valid_currencies.size());
    for (const auto& currency : finance::valid_currencies) {
        output[currency] = initValue;
    }
    return output;
}
