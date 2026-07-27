// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <span>
#include <string>
#include <vector>

#include "finapp/finance/asset/AssetType.hpp"
#include "finapp/finance/calendar/AssetEvent.hpp"
#include "finapp/finance/calendar/Recurrence.hpp"
#include "finapp/finance/calendar/TradingCalendar.hpp"
#include "finapp/finance/common/Currency.hpp"
#include "finapp/finance/portfolio/Transaction.hpp"

namespace finance {

// A sorted timeline of per-unit AssetEvents for one asset (dividends, splits, coupons). Kept
// ascending by timestamp. The bridge to the portfolio is projection: events in a window are turned
// into Transactions and replayed through Portfolio::apply — the same path live transactions use, so
// historical replay and Monte-Carlo forward modeling share it.
class EventCalendar {
 public:
    // Insert an event, keeping the timeline sorted by timestamp (stable for equal timestamps).
    void add(const AssetEvent& event);

    // Generate recurring events (e.g. a quarterly dividend) on trading days per the recurrence's
    // calendar, marked projected = true. amountPerUnit is $/unit (or split ratio).
    void addRecurring(const Recurrence& recurrence, AssetEventKind kind, double amountPerUnit,
                      Currency currency, const TradingCalendar& calendar);

    std::span<const AssetEvent> events() const { return events_; }

    // Events with ts in [start, end], ascending.
    std::vector<AssetEvent> inRange(Timestamp start, Timestamp end) const;

    // Project events in [start, end] into Transactions for a holding of (ticker, assetType),
    // ready for Portfolio::apply. Held quantity is applied inside apply() — not here.
    std::vector<Transaction> project(const std::string& ticker, AssetType assetType, Timestamp start,
                                     Timestamp end) const;

 private:
    std::vector<AssetEvent> events_;  // ascending by ts
};
}  // namespace finance
