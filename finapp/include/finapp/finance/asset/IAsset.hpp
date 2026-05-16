// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <string>

#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/common/Currency.hpp"
namespace finance {

class IAsset {
 public:
    IAsset() = default;
    virtual ~IAsset() = default;
    IAsset(const IAsset&) = default;
    IAsset& operator=(const IAsset&) = default;
    IAsset(IAsset&&) = default;
    IAsset& operator=(IAsset&&) = default;

    virtual std::string ticker() const = 0;
    virtual std::string name() const = 0;
    virtual AssetType type() const = 0;
    virtual finance::Currency denomination() const = 0;

    virtual std::string priceSeriesId() const { return ticker(); }
};

}  // namespace finance
