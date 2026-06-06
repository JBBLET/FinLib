// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>

#include "finapp/data/repository/interface/IFXRepository.hpp"
#include "finapp/finance/common/Currency.hpp"

namespace finapp {

class InMemoryFXRepository : public IFXRepository {
 public:
    FXInfos load(const finance::Currency& base, const finance::Currency& quote) const override {
        auto it = entries_.find(key_(base, quote));
        if (it == entries_.end()) {
            throw std::runtime_error("InMemoryFXRepository: pair not found");
        }
        return it->second;
    }

    void save(const FXInfos& info) override {
        entries_.insert_or_assign(key_(info.baseCurrency, info.quoteCurrency), info);
    }

    bool exists(const finance::Currency& base, const finance::Currency& quote) const override {
        return entries_.contains(key_(base, quote));
    }

    size_t size() const { return entries_.size(); }

 private:
    static std::string key_(finance::Currency base, finance::Currency quote) {
        return toString(base) + "/" + toString(quote);
    }
    std::unordered_map<std::string, FXInfos> entries_;
};

}  // namespace finapp
