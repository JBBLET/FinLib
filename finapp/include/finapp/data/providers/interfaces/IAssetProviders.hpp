// Copyright (c) 2026 JBBLET. All Rights Reserved.

#pragma once

#include <memory>
#include <string>

#include "finapp/finance/asset/IAsset.hpp"
namespace finance {
class IAssetProvider {
 public:
    virtual ~IAssetProvider() = default;
    virtual std::shared_ptr<IAsset> fetch(const std::string& ticker) const = 0;

    virtual bool exists(const std::string& ticker) const = 0;
};
}  // namespace finance
