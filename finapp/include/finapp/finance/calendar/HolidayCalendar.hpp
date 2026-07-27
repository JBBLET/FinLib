// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <chrono>
#include <cstdint>
#include <span>
#include <unordered_set>
#include <vector>

#include "finapp/finance/calendar/CalendarBase.hpp"

namespace finance {

// An exchange trading calendar: Monday–Friday minus a holiday table (e.g. JPX / NYSE national
// bank holidays). Construct from a list of holiday instants; each is bucketed to its local civil
// day using the exchange's utcOffsetMs, so the same file works regardless of the timestamps'
// time-of-day. roll/schedule/periodsPerYear (annualization factor) come from CalendarBase and
// automatically reflect the holidays — no per-market tuning.
//
// Load pattern: read holidays from a per-exchange source (e.g. a CSV via finapp_csv_repository)
// into a vector<Timestamp> and hand it here, analogous to how assets load prices.
class HolidayCalendar : public CalendarBase {
 public:
    explicit HolidayCalendar(std::span<const Timestamp> holidays, Timestamp utcOffsetMs = 0)
        : CalendarBase(utcOffsetMs) {
        for (const Timestamp h : holidays) holidayDays_.insert(localDayNumber_(h));
    }

    bool isTradingDay(Timestamp ts) const override {
        const auto wd = localWeekday_(ts);
        if (wd == std::chrono::Saturday || wd == std::chrono::Sunday) return false;
        return !holidayDays_.contains(localDayNumber_(ts));
    }

    // Add a holiday after construction (interpreted in the exchange's local zone).
    void addHoliday(Timestamp ts) { holidayDays_.insert(localDayNumber_(ts)); }

    std::size_t holidayCount() const { return holidayDays_.size(); }

 private:
    std::unordered_set<std::int64_t> holidayDays_;
};
}  // namespace finance
