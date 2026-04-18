// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <memory>
#include <string>

#include "finapp/data/providers/interfaces/IAssetProviders.hpp"

namespace finapp {

// Fetches Equity metadata (name, currency, exchange, sector) from yfinance.
// Implements IAssetProvider; inject into AssetService for AssetType::Equity.
class YFinanceEquityProvider : public IAssetProvider {
 public:
    YFinanceEquityProvider() = default;

    // Returns a heap-allocated Equity built from yf.Ticker(ticker).info.
    // Throws std::out_of_range if yfinance returns an unsupported currency code.
    // Throws pybind11::error_already_set on any Python-side exception.
    std::shared_ptr<finance::IAsset> fetch(const std::string& ticker) const override;

    // Returns true if yfinance can resolve the ticker to a named security.
    // Makes a network call — do not call in a hot path.
    bool exists(const std::string& ticker) const override;
};

}  // namespace finapp
