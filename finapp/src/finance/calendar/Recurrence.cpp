// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finapp/finance/calendar/Recurrence.hpp"

#include <algorithm>
#include <chrono>

namespace finance {
namespace {
std::chrono::sys_days toDays(Timestamp ts) {
    return std::chrono::floor<std::chrono::days>(
        std::chrono::sys_time<std::chrono::milliseconds>{std::chrono::milliseconds{ts}});
}

Timestamp daysToMs(std::chrono::sys_days d) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(d.time_since_epoch()).count();
}

// year/month + n months, keeping day-of-month but clamping to the month's last day
// (e.g. Jan 31 + 1 month -> Feb 28/29).
Timestamp addMonths(std::chrono::year_month_day ymd0, int monthsToAdd, Timestamp intradayMs) {
    const std::chrono::year_month ym =
        std::chrono::year_month{ymd0.year(), ymd0.month()} + std::chrono::months{monthsToAdd};
    std::chrono::year_month_day cand{ym.year(), ym.month(), ymd0.day()};
    if (!cand.ok()) cand = std::chrono::year_month_day{ym.year() / ym.month() / std::chrono::last};
    return daysToMs(std::chrono::sys_days{cand}) + intradayMs;
}
}  // namespace

std::vector<Timestamp> Recurrence::generateRaw() const {
    std::vector<Timestamp> dates;
    if (interval < 1 || end < start) return dates;

    const std::chrono::sys_days day0 = toDays(start);
    const Timestamp intradayMs = start - daysToMs(day0);
    const std::chrono::year_month_day ymd0{day0};

    for (int k = 0;; ++k) {
        Timestamp ms;
        switch (frequency) {
            case Frequency::Daily:
                ms = daysToMs(day0 + std::chrono::days{static_cast<long>(interval) * k}) + intradayMs;
                break;
            case Frequency::Weekly:
                ms = daysToMs(day0 + std::chrono::days{7L * interval * k}) + intradayMs;
                break;
            case Frequency::Monthly:
                ms = addMonths(ymd0, interval * k, intradayMs);
                break;
            case Frequency::Quarterly:
                ms = addMonths(ymd0, 3 * interval * k, intradayMs);
                break;
            case Frequency::Yearly:
                ms = addMonths(ymd0, 12 * interval * k, intradayMs);
                break;
            default:
                ms = start;
                break;
        }
        if (ms > end) break;
        dates.push_back(ms);
    }
    return dates;
}

std::vector<Timestamp> Recurrence::generate(const TradingCalendar& calendar) const {
    std::vector<Timestamp> dates = generateRaw();
    for (Timestamp& d : dates) d = calendar.roll(d, roll);
    std::sort(dates.begin(), dates.end());
    dates.erase(std::unique(dates.begin(), dates.end()), dates.end());
    return dates;
}
}  // namespace finance
