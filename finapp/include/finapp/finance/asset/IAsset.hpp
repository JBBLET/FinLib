// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once
#include <memory>
#include <string>

#include "finapp/finance/common/Currency.hpp"

namespace finance {

enum class AssetType : uint8_t { Equity, ETF, Bond, Cash };

class IAsset {
 public:
    virtual ~IAsset() = default;

    virtual std::string ticker() const = 0;
    virtual std::string name() const = 0;
    virtual AssetType type() const = 0;
    virtual finance::Currency denomination() const = 0;

    virtual std::string priceSeriesId() const { return ticker(); }
};

struct Position {
    std::shared_ptr<const IAsset> asset;
    double quantity;
};
}  // namespace finance
