// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "finapp/data/repository/interface/IAssetRepository.hpp"
#include "finapp/finance/asset/IAsset.hpp"

namespace finapp {

class InMemoryAssetRepository : public IAssetRepository {
 public:
    void save(const std::shared_ptr<const finance::IAsset>& asset) override {
        if (!asset) throw std::invalid_argument("InMemoryAssetRepository: null asset");
        assets_[asset->ticker()] = asset;
    }

    std::shared_ptr<const finance::IAsset> load(const std::string& ticker) const override {
        auto it = assets_.find(ticker);
        return it == assets_.end() ? nullptr : it->second;
    }

    bool exists(const std::string& ticker) const override { return assets_.contains(ticker); }

    std::vector<std::string> listTickers() const override {
        std::vector<std::string> out;
        out.reserve(assets_.size());
        for (const auto& [t, _] : assets_) out.push_back(t);
        return out;
    }

    std::unordered_map<std::string, std::shared_ptr<const finance::IAsset>> loadAll(
        const std::vector<std::string>& tickers) const override {
        std::unordered_map<std::string, std::shared_ptr<const finance::IAsset>> out;
        for (const auto& t : tickers) {
            auto it = assets_.find(t);
            if (it != assets_.end()) out.emplace(t, it->second);
        }
        return out;
    }

    size_t count() const { return assets_.size(); }

 private:
    std::unordered_map<std::string, std::shared_ptr<const finance::IAsset>> assets_;
};

}  // namespace finapp
