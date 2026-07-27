// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstdint>
#include <vector>

#include "finapp/finance/calendar/TradingCalendar.hpp"

namespace finance {

enum class Frequency : std::uint8_t {
    Daily,
    Weekly,
    Monthly,
    Quarterly,
    Yearly,
};

// A rule that generates a sequence of dates: start, start + interval*frequency, ... up to and
// including end. Each generated date is rolled onto a trading calendar per `roll`, so recurring
// contributions / dividends land on valid trading days. Anchored on start's day-of-month for
// Monthly/Quarterly/Yearly (clamped to month end, e.g. the 31st becomes Feb 28/29).
//
// Used both for portfolio cash flows (planned Deposit/Withdrawal Transactions) and for modeled
// asset events (EventCalendar::addRecurring).
struct Recurrence {
    Timestamp start;
    Timestamp end;  // inclusive upper bound on the *unrolled* date
    Frequency frequency;
    int interval = 1;  // every N periods (N >= 1)
    RollConvention roll = RollConvention::Following;

    // Rolled, ascending, duplicate-free trading-day timestamps. Rolling can collapse two nearby
    // unrolled dates onto the same trading day; those duplicates are removed.
    std::vector<Timestamp> generate(const TradingCalendar& calendar) const;

    // The raw dates before any calendar rolling (ascending, exact civil steps).
    std::vector<Timestamp> generateRaw() const;
};
}  // namespace finance
