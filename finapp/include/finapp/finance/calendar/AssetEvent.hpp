// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <string>

#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/calendar/TradingCalendar.hpp"  // Timestamp
#include "finapp/finance/common/Currency.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"

namespace finance {

enum class AssetEventKind : std::uint8_t {
    Dividend,  // amountPerUnit = cash per share
    Split,     // amountPerUnit = split ratio (2.0 == 2-for-1)
    Coupon,    // amountPerUnit = cash per unit (bond coupon), treated like a dividend
};

// A per-unit, holder-independent corporate action attached to an asset (not a Transaction — a
// dividend is $/share with no holder). The held quantity is applied later, inside Portfolio::apply,
// when this is projected to a Transaction. See projectToTransaction.
struct AssetEvent {
    Timestamp ts;
    AssetEventKind kind;
    double amountPerUnit;  // $/unit for Dividend/Coupon; ratio for Split
    Currency currency;
    bool projected = false;  // false = declared/historical; true = modeled-forward (e.g. Monte Carlo)
};

// Project a per-unit event onto a Transaction for a specific holding. Does NOT multiply by the held
// quantity — Portfolio::applyDividend_ multiplies by held shares and applySplit_ uses quantity as the
// ratio, so multiplying here would double-count. Dividend/Coupon -> Dividend Transaction carrying
// amountPerUnit as pricePerUnit; Split -> Split Transaction carrying the ratio as quantity.
Transaction projectToTransaction(const AssetEvent& event, const std::string& ticker, AssetType assetType);
}  // namespace finance
