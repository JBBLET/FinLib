// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <chrono>
#include <memory>
#include <vector>

#include "finapp/finance/calendar/TradingCalendar.hpp"

namespace finance {

// Reusable engine for calendars: implements roll / schedule / periodsPerYear purely in terms
// of the derived isTradingDay, plus the civil-date helpers. A concrete calendar only overrides
// isTradingDay (e.g. weekend rule, holiday table).
//
// Timezone: exchanges keep holidays in their local zone. utcOffsetMs shifts an instant into the
// exchange's local civil day before it is bucketed, so a Tokyo calendar tests the Tokyo date and
// its schedule ticks land on local midnight (expressed in UTC). Offset 0 == pure UTC (the default,
// which reproduces the original WeekdayCalendar behavior exactly).
class CalendarBase : public TradingCalendar {
 public:
    Timestamp roll(Timestamp ts, RollConvention c) const override {
        switch (c) {
            case RollConvention::Unadjusted:
                return ts;
            case RollConvention::Following:
                return adjust_(ts, +1);
            case RollConvention::Preceding:
                return adjust_(ts, -1);
            case RollConvention::ModifiedFollowing: {
                const Timestamp r = adjust_(ts, +1);
                return sameMonth_(r, ts) ? r : adjust_(ts, -1);
            }
            case RollConvention::ModifiedPreceding: {
                const Timestamp r = adjust_(ts, -1);
                return sameMonth_(r, ts) ? r : adjust_(ts, +1);
            }
        }
        return ts;
    }

    // One tick per trading day in [start, end], aligned to local midnight (in UTC).
    TimestampsPtr schedule(Timestamp start, Timestamp end) const override {
        auto out = std::make_shared<std::vector<Timestamp>>();
        for (Timestamp d = floorDay_(start); d <= end; d += kMsPerDay_)
            if (isTradingDay(d)) out->push_back(d);
        return out;
    }

    double periodsPerYear(Timestamp start, Timestamp end) const override {
        if (end <= start) return 0.0;
        const double years = static_cast<double>(end - start) / kMsPerYear_;
        return static_cast<double>(schedule(start, end)->size()) / years;
    }

 protected:
    explicit CalendarBase(Timestamp utcOffsetMs = 0) : utcOffsetMs_{utcOffsetMs} {}

    static constexpr Timestamp kMsPerDay_ = 86'400'000LL;
    static constexpr double kMsPerYear_ = 365.2425 * 86'400'000.0;

    // The local civil day an instant falls on (shifted by the exchange offset).
    std::chrono::year_month_day localDate_(Timestamp ts) const {
        return std::chrono::year_month_day{localDay_(ts)};
    }
    std::chrono::weekday localWeekday_(Timestamp ts) const {
        return std::chrono::weekday{localDay_(ts)};
    }
    // Days since the epoch of the local civil day — a stable key for holiday-set membership.
    std::int64_t localDayNumber_(Timestamp ts) const {
        return static_cast<std::int64_t>(localDay_(ts).time_since_epoch().count());
    }

 private:
    std::chrono::sys_days localDay_(Timestamp ts) const {
        return std::chrono::floor<std::chrono::days>(
            std::chrono::sys_time<std::chrono::milliseconds>{std::chrono::milliseconds{ts + utcOffsetMs_}});
    }
    // The UTC instant of the local day's midnight containing ts.
    Timestamp floorDay_(Timestamp ts) const {
        const Timestamp localMidnightAsUtc =
            std::chrono::duration_cast<std::chrono::milliseconds>(localDay_(ts).time_since_epoch()).count();
        return localMidnightAsUtc - utcOffsetMs_;
    }
    bool sameMonth_(Timestamp a, Timestamp b) const {
        const auto ya = localDate_(a);
        const auto yb = localDate_(b);
        return ya.year() == yb.year() && ya.month() == yb.month();
    }
    Timestamp adjust_(Timestamp ts, int dir) const {
        Timestamp t = ts;
        while (!isTradingDay(t)) t += dir * kMsPerDay_;
        return t;
    }

    Timestamp utcOffsetMs_;
};
}  // namespace finance
