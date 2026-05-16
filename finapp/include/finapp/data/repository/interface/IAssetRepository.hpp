// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "finapp/finance/asset/IAsset.hpp"

namespace finapp {

class IAssetRepository {
 public:
    IAssetRepository() = default;
    virtual ~IAssetRepository() = default;
    IAssetRepository(const IAssetRepository&) = default;
    IAssetRepository& operator=(const IAssetRepository&) = default;
    IAssetRepository(IAssetRepository&&) = default;
    IAssetRepository& operator=(IAssetRepository&&) = default;

    virtual void save(const std::shared_ptr<const finance::IAsset>& asset) = 0;
    virtual std::shared_ptr<const finance::IAsset> load(const std::string& ticker) const = 0;
    virtual bool exists(const std::string& ticker) const = 0;
    virtual std::vector<std::string> listTickers() const = 0;

    virtual std::unordered_map<std::string, std::shared_ptr<const finance::IAsset>> loadAll(
        const std::vector<std::string>& tickers) const = 0;
};

}  // namespace finapp
