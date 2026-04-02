// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <string>

#include "finapp/finance/common/Currency.hpp"

namespace finance {

struct FXInfos {
    Currency baseCurrency;
    Currency quoteCurrency;
    std::string timeseriesID;
};

class IFXRepository {
 public:
    virtual ~IFXRepository() = default;

    virtual FXInfos load(const Currency& baseCurrency, const Currency& quoteCurrency) const = 0;
    virtual void save(const FXInfos& fxInfos) = 0;
    virtual bool exists(const Currency& baseCurrency, const Currency& quoteCurrency) const = 0;
};
}  // namespace finance
