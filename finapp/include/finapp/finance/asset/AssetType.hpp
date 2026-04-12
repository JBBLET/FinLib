// Copyright (c) 2026 JBBLET. All Rights Reserved.

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace finance {

enum class AssetType : uint8_t { Equity, ETF, Bond, Cash, Crypto };

static std::unordered_map<std::string, AssetType> assetTypeName = {{"Equity", AssetType::Equity},
                                                                   {"ETF", AssetType::ETF},
                                                                   {"Bond", AssetType::Bond},
                                                                   {"Cash", AssetType::Cash},
                                                                   {"Crypto", AssetType::Crypto}};

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

inline const AssetType assetTypeFromString(const std::string& name) { return assetTypeName[name]; }

}  // namespace finance
