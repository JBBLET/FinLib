// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <string>

#include "finapp/finance/common/Currency.hpp"

namespace finapp {

struct FXInfos {
    finance::Currency baseCurrency;
    finance::Currency quoteCurrency;
    std::string timeseriesID;
};

class IFXRepository {
 public:
    IFXRepository() = default;
    virtual ~IFXRepository() = default;
    IFXRepository(const IFXRepository&) = default;
    IFXRepository& operator=(const IFXRepository&) = default;
    IFXRepository(IFXRepository&&) = default;
    IFXRepository& operator=(IFXRepository&&) = default;

    virtual FXInfos load(const finance::Currency& baseCurrency, const finance::Currency& quoteCurrency) const = 0;
    virtual void save(const FXInfos& fxInfos) = 0;
    virtual bool exists(const finance::Currency& baseCurrency, const finance::Currency& quoteCurrency) const = 0;
};
}  // namespace finapp
