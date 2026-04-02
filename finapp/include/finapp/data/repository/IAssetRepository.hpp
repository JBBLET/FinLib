// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "finapp/finance/asset/IAsset.hpp"

namespace finance {

class IAssetRepository {
 public:
    virtual ~IAssetRepository() = default;
    virtual void save(const std::shared_ptr<const IAsset>& asset) = 0;
    virtual std::shared_ptr<const IAsset> load(const std::string& ticker) const = 0;
    virtual bool exists(const std::string& ticker) const = 0;
    virtual std::vector<std::string> listTickers() const = 0;

    virtual std::unordered_map<std::string, std::shared_ptr<const IAsset>> loadAll(
        const std::vector<std::string>& tickers) const = 0;
};

}  // namespace finance
