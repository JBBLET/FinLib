// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/finance/asset/AssetType.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace finance {

namespace {
const std::unordered_map<std::string, AssetType> nameToAssetTypeMap = {{"Equity", AssetType::Equity},
                                                                       {"ETF", AssetType::ETF},
                                                                       {"Bond", AssetType::Bond},
                                                                       {"Cash", AssetType::Cash},
                                                                       {"Crypto", AssetType::Crypto}};
}  // namespace

const std::string assetTypeToString(AssetType assetType) {
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

AssetType assetTypeFromString(const std::string& name) {
    auto it = nameToAssetTypeMap.find(name);
    if (it == nameToAssetTypeMap.end()) throw std::runtime_error("Unsupported asset type: '" + name + "'");
    return it->second;
}

}  // namespace finance
