// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "finlib/common/FinlibTypes.hpp"

namespace ts::fmt {

// ---------------------------------------------------------------------------
// Format spec
// ---------------------------------------------------------------------------
// Every finlib type that opts into std::format understands the same grammar, so
// one spelling works everywhere:
//
//   {}      identity  — a single line, safe to embed in an ensure() message or a log line
//   {:r}    repr      — head, ellipsis, tail; the interactive default
//   {:h}    head      — first N rows          ({:h20} for twenty)
//   {:t}    tail      — last N rows           ({:t20})
//   {:d}    describe  — the statistics table
//

enum class FormatMode { Identity, Repr, Head, Tail, Describe };

struct FormatSpec {
    FormatMode mode = FormatMode::Identity;
    std::size_t count = 10;  // rows shown by Head / Tail, and the window either side of Repr's ellipsis
    int precision = -1;      // -1 = derive from the data
};

// Shared parse() body for every finlib formatter.
template <class ParseContext>
constexpr typename ParseContext::iterator parseFormatSpec(ParseContext& ctx, FormatSpec& spec) {
    auto it = ctx.begin();
    if (it == ctx.end() || *it == '}') return it;

    switch (*it) {
        case 's':
            spec.mode = FormatMode::Identity;
            ++it;
            break;
        case 'r':
            spec.mode = FormatMode::Repr;
            ++it;
            break;
        case 'h':
            spec.mode = FormatMode::Head;
            ++it;
            break;
        case 't':
            spec.mode = FormatMode::Tail;
            ++it;
            break;
        case 'd':
            spec.mode = FormatMode::Describe;
            ++it;
            break;
        default:
            break;
    }

    if (it != ctx.end() && *it >= '0' && *it <= '9') {
        std::size_t n = 0;
        while (it != ctx.end() && *it >= '0' && *it <= '9') n = n * 10 + static_cast<std::size_t>(*it++ - '0');
        spec.count = n;
    }

    if (it != ctx.end() && *it == '.') {
        ++it;
        int p = 0;
        while (it != ctx.end() && *it >= '0' && *it <= '9') p = p * 10 + (*it++ - '0');
        spec.precision = p;
    }

    if (it != ctx.end() && *it != '}') throw std::format_error("finlib: expected one of s/r/h/t/d[count][.precision]");
    return it;
}

// ---------------------------------------------------------------------------
// Numbers
// ---------------------------------------------------------------------------

std::string formatDouble(double v, int precision = -1);

// One precision for a whole column so the figures line up on the decimal point.
struct ColumnFormat {
    int precision = 2;
    bool scientific = false;

    std::string operator()(double v) const;
};

ColumnFormat columnFormat(std::span<const double> values, int forcedPrecision = -1);

// ---------------------------------------------------------------------------
// Optionals
// ---------------------------------------------------------------------------

template <class T>
struct Opt {
    const std::optional<T>& value;
    std::string_view fallback = "N/A";
};
template <class T>
Opt(const std::optional<T>&) -> Opt<T>;

std::string naOr(const std::optional<double>& v, int precision = -1, std::string_view fallback = "N/A");

// ---------------------------------------------------------------------------
// Timestamps
// ---------------------------------------------------------------------------

struct AsDate {
    Timestamp ms;
};
struct AsDateTime {
    Timestamp ms;
};

// A millisecond span written the way a frequency is spoken: 86'400'000 -> "1d". Falls back
// to raw milliseconds when the value is not a whole number of any unit, so nothing is ever
// rounded away silently.
std::string formatDuration(Timestamp ms);

// ---------------------------------------------------------------------------
// Table
// ---------------------------------------------------------------------------

class Table {
 public:
    enum class Align { Left, Right };

    Table(std::vector<std::string> headers, std::vector<Align> alignment);
    explicit Table(std::vector<std::string> headers);

    void addRow(std::vector<std::string> cells);
    void addRule();

    std::string render() const;

 private:
    std::vector<std::string> headers_;
    std::vector<Align> alignment_;
    std::vector<std::optional<std::vector<std::string>>> rows_;  // nullopt = horizontal rule
};

// ---------------------------------------------------------------------------
// Series rendering
// ---------------------------------------------------------------------------
std::string renderSeries(std::string_view identity, std::span<const Timestamp> timestamps,
                         std::span<const double> values, const FormatSpec& spec);

// count / non-finite / mean / std / min / quartiles / max, in the pandas order.
std::string renderDescribe(std::string_view identity, std::span<const double> values, int precision = -1);

}  // namespace ts::fmt

// ---------------------------------------------------------------------------
// std::formatter specializations
// ---------------------------------------------------------------------------

template <>
struct std::formatter<ts::fmt::AsDate> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto format(const ts::fmt::AsDate& d, std::format_context& ctx) const -> std::format_context::iterator;
};

template <>
struct std::formatter<ts::fmt::AsDateTime> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto format(const ts::fmt::AsDateTime& d, std::format_context& ctx) const -> std::format_context::iterator;
};

template <class T>
struct std::formatter<ts::fmt::Opt<T>> {
    std::formatter<T> inner;

    constexpr auto parse(std::format_parse_context& ctx) { return inner.parse(ctx); }

    template <class Ctx>
    auto format(const ts::fmt::Opt<T>& o, Ctx& ctx) const {
        if (!o.value.has_value()) return std::format_to(ctx.out(), "{}", o.fallback);
        return inner.format(*o.value, ctx);
    }
};
