// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <stdexcept>
#include <string>

#include "finapp/finance/common/Currency.hpp"

namespace finance {

enum class AssetType : uint8_t { Equity, ETF, Bond, Cash, Crypto };

inline const std::string assetTypeToString(AssetType assetType) {
    switch (assetType) {
        case AssetType::Equity:
            return "Equity";
        case AssetType::ETF:
            return "ETF";
        case AssetType::Bond:
            return "Bond";
        case AssetType::Cash:
            return "Cash";
        case AssetType::Crypto:
            return "Crypto";
        default:
            throw std::runtime_error("Illegal asset Type");
    }
}

class IAsset {
 public:
    virtual ~IAsset() = default;

    virtual std::string ticker() const = 0;
    virtual std::string name() const = 0;
    virtual AssetType type() const = 0;
    virtual finance::Currency denomination() const = 0;

    virtual std::string priceSeriesId() const { return ticker(); }
};

}  // namespace finance
