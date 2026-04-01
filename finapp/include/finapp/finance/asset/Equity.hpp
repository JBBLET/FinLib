// Copyright (c) 2026 JBBLET. All Rights Reserved.

#pragma once
#include <string>
#include <utility>

#include "finapp/finance/asset/IAsset.hpp"
#include "finapp/finance/common/Currency.hpp"

namespace finance {

class Equity : public IAsset {
 public:
    Equity(std::string ticker, std::string name, Currency denom, std::string exchange = "", std::string sector = "")
        : ticker_(std::move(ticker)),
          name_(std::move(name)),
          denom_(denom),
          exchange_(std::move(exchange)),
          sector_(std::move(sector)) {}
    std::string ticker() const override { return ticker_; }
    std::string name() const override { return name_; }
    AssetType type() const override { return AssetType::Equity; }
    Currency denomination() const override { return denom_; }

    const std::string& exchange() const { return exchange_; }
    const std::string& sector() const { return sector_; }

 private:
    std::string ticker_;
    std::string name_;
    Currency denom_;
    std::string exchange_;
    std::string sector_;
};
}  //  namespace finance
