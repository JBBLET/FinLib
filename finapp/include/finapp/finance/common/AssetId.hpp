// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "finapp/finance/asset/IAsset.hpp"

namespace finance {

struct AssetId {
    AssetType type;
    std::string ticker;
    bool operator==(const AssetId&) const = default;
};
}  // namespace finance

template <>
struct std::hash<finance::AssetId> {
    std::size_t operator()(const finance::AssetId& assetId) const {
        std::size_t result = 12;
        result = result * 17 + std::hash<uint8_t>()(static_cast<uint8_t>(assetId.type));
        result = result * 17 + std::hash<std::string>()(assetId.ticker);
        return result;
    }
};
