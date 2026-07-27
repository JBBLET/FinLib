// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/finance/calendar/EventCalendar.hpp"

#include <algorithm>

#include "finapp/finance/calendar/AssetEvent.hpp"

namespace finance {

Transaction projectToTransaction(const AssetEvent& event, const std::string& ticker, AssetType assetType) {
    switch (event.kind) {
        case AssetEventKind::Dividend:
        case AssetEventKind::Coupon:
            // applyDividend_ multiplies pricePerUnit by the held quantity — leave quantity 0 here.
            return Transaction{.timestampsMs = event.ts,
                               .type = TransactionType::Dividend,
                               .assetType = assetType,
                               .assetTicker = ticker,
                               .quantity = 0.0,
                               .pricePerUnit = event.amountPerUnit,
                               .fees = 0.0,
                               .settlementCurrency = event.currency};
        case AssetEventKind::Split:
            // applySplit_ uses quantity as the split ratio.
            return Transaction{.timestampsMs = event.ts,
                               .type = TransactionType::Split,
                               .assetType = assetType,
                               .assetTicker = ticker,
                               .quantity = event.amountPerUnit,
                               .pricePerUnit = 0.0,
                               .fees = 0.0,
                               .settlementCurrency = event.currency};
    }
    return Transaction{.timestampsMs = event.ts,
                       .type = TransactionType::Dividend,
                       .assetType = assetType,
                       .assetTicker = ticker,
                       .quantity = 0.0,
                       .pricePerUnit = event.amountPerUnit,
                       .fees = 0.0,
                       .settlementCurrency = event.currency};
}

void EventCalendar::add(const AssetEvent& event) {
    const auto pos = std::upper_bound(events_.begin(), events_.end(), event.ts,
                                      [](Timestamp ts, const AssetEvent& e) { return ts < e.ts; });
    events_.insert(pos, event);
}

void EventCalendar::addRecurring(const Recurrence& recurrence, AssetEventKind kind, double amountPerUnit,
                                 Currency currency, const TradingCalendar& calendar) {
    for (const Timestamp ts : recurrence.generate(calendar))
        add(AssetEvent{.ts = ts, .kind = kind, .amountPerUnit = amountPerUnit, .currency = currency, .projected = true});
}

std::vector<AssetEvent> EventCalendar::inRange(Timestamp start, Timestamp end) const {
    std::vector<AssetEvent> out;
    for (const AssetEvent& e : events_)
        if (e.ts >= start && e.ts <= end) out.push_back(e);
    return out;
}

std::vector<Transaction> EventCalendar::project(const std::string& ticker, AssetType assetType, Timestamp start,
                                                Timestamp end) const {
    std::vector<Transaction> out;
    for (const AssetEvent& e : events_)
        if (e.ts >= start && e.ts <= end) out.push_back(projectToTransaction(e, ticker, assetType));
    return out;
}
}  // namespace finance
