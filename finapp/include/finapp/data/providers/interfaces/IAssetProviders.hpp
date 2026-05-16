// Copyright (c) 2026 JBBLET. All Rights Reserved.

#pragma once

#include <memory>
#include <string>

#include "finapp/finance/asset/IAsset.hpp"
namespace finapp {
class IAssetProvider {
 public:
    IAssetProvider() = default;
    virtual ~IAssetProvider() = default;
    IAssetProvider(const IAssetProvider&) = default;
    IAssetProvider& operator=(const IAssetProvider&) = default;
    IAssetProvider(IAssetProvider&&) = default;
    IAssetProvider& operator=(IAssetProvider&&) = default;

    virtual std::shared_ptr<finance::IAsset> fetch(const std::string& ticker) const = 0;

    virtual bool exists(const std::string& ticker) const = 0;
};
}  // namespace finapp
