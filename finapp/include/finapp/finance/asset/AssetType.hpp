// Copyright (c) 2026 JBBLET. All Rights Reserved.

#pragma once

#include <cstdint>
#include <string>

namespace finance {

enum class AssetType : uint8_t { Equity, ETF, Bond, Cash, Crypto };

// Canonical name for an asset type. Throws std::runtime_error on an unknown enumerator.
const std::string assetTypeToString(AssetType assetType);

// Parses an asset-type name. Throws std::runtime_error on an unknown name.
AssetType assetTypeFromString(const std::string& name);

}  // namespace finance
