// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once
#include <string>

#include "finapp/finance/asset/IAsset.hpp"
#include "finapp/finance/common/Currency.hpp"
namespace finance {

class Cash : public IAsset {
 public:
    explicit Cash(Currency currency) : currency_(currency) {}
    std::string ticker() const override { return toString(currency_) + " Cash"; }
    AssetType type() const override { return AssetType::Cash; }
    Currency denomination() const override { return currency_; }
    std::string name() const override { return toString(currency_); }
    std::string priceSeriesId() const override { return ""; }

 private:
    Currency currency_;
};
}  // namespace finance
