// Copyright (c) 2026 JBBLET. All Rights Reserved.
#include "finlib/common/Format.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "finlib/common/FinlibTypes.hpp"
#include "finlib/common/utils/TimeUtils.hpp"
#include "finlib/core/StatsCore.hpp"

namespace ts::fmt {
namespace {

// Outside this band a fixed-point rendering is either unreadably long or all zeros.
constexpr double kScientificAbove = 1e7;
constexpr double kScientificBelow = 1e-5;

// Roughly five significant digits: 431.2 -> 2 decimals, 0.0312 -> 6.
int decimalsFor(double magnitude) {
    if (!(magnitude > 0.0)) return 2;
    const int exponent = static_cast<int>(std::floor(std::log10(magnitude)));
    return std::clamp(4 - exponent, 2, 8);
}

std::string nonFinite(double v) { return std::isnan(v) ? "NaN" : (v > 0 ? "inf" : "-inf"); }

}  // namespace

// ---------------------------------------------------------------------------
// Numbers
// ---------------------------------------------------------------------------

std::string formatDouble(double v, int precision) {
    if (!std::isfinite(v)) return nonFinite(v);
    if (precision >= 0) return std::format("{:.{}f}", v, precision);

    const double a = std::abs(v);
    if (a != 0.0 && (a >= kScientificAbove || a < kScientificBelow)) return std::format("{:.4e}", v);

    // decimalsFor budgets for the worst case at this magnitude, which leaves padding zeros on
    // values that do not need them (0.00042 -> 0.00042000). Those are load-bearing in a table,
    // where the column must line up, so ColumnFormat keeps them; inline they are just noise.
    std::string out = std::format("{:.{}f}", v, decimalsFor(a));
    if (out.find('.') != std::string::npos) {
        out.erase(out.find_last_not_of('0') + 1);
        if (out.back() == '.') out.pop_back();
    }
    return out;
}

std::string ColumnFormat::operator()(double v) const {
    if (!std::isfinite(v)) return nonFinite(v);
    if (scientific) return std::format("{:.{}e}", v, precision);
    return std::format("{:.{}f}", v, precision);
}

ColumnFormat columnFormat(std::span<const double> values, int forcedPrecision) {
    ColumnFormat format;
    if (forcedPrecision >= 0) {
        format.precision = forcedPrecision;
        return format;
    }

    double maxAbs = 0.0;
    for (double v : values) {
        if (std::isfinite(v)) maxAbs = std::max(maxAbs, std::abs(v));
    }

    if (maxAbs == 0.0) return format;  // all zero, all non-finite, or empty
    if (maxAbs >= kScientificAbove || maxAbs < kScientificBelow) {
        format.scientific = true;
        format.precision = 4;
        return format;
    }
    format.precision = decimalsFor(maxAbs);
    return format;
}

std::string formatDuration(Timestamp ms) {
    if (ms == 0) return "0ms";
    if (ms < 0) return "-" + formatDuration(-ms);

    struct Unit {
        Timestamp size;
        std::string_view suffix;
    };
    static constexpr Unit kUnits[] = {
        {604'800'000, "w"}, {86'400'000, "d"}, {3'600'000, "h"}, {60'000, "m"}, {1'000, "s"}, {1, "ms"},
    };

    // Two components is the readable limit: "4h48m" rather than either "288m" or a full
    // "4h48m0s0ms". A remainder below the second unit is dropped, so this is a label, not
    // a lossless encoding — callers needing exactness print the raw count.
    std::string out;
    int emitted = 0;
    for (const auto& [size, suffix] : kUnits) {
        if (ms < size) continue;
        out += std::format("{}{}", ms / size, suffix);
        ms %= size;
        if (++emitted == 2 || ms == 0) break;
    }
    return out;
}

std::string naOr(const std::optional<double>& v, int precision, std::string_view fallback) {
    if (!v.has_value()) return std::string{fallback};
    return formatDouble(*v, precision);
}

// ---------------------------------------------------------------------------
// Table
// ---------------------------------------------------------------------------

Table::Table(std::vector<std::string> headers, std::vector<Align> alignment)
    : headers_(std::move(headers)), alignment_(std::move(alignment)) {
    alignment_.resize(headers_.size(), Align::Right);
}

Table::Table(std::vector<std::string> headers) : Table(std::move(headers), {}) {}

void Table::addRow(std::vector<std::string> cells) { rows_.emplace_back(std::move(cells)); }

void Table::addRule() { rows_.emplace_back(std::nullopt); }

std::string Table::render() const {
    const std::size_t columns = headers_.size();
    if (columns == 0) return {};

    std::vector<std::size_t> width(columns);
    for (std::size_t c = 0; c < columns; ++c) width[c] = headers_[c].size();
    for (const auto& row : rows_) {
        if (!row) continue;
        for (std::size_t c = 0; c < columns && c < row->size(); ++c) width[c] = std::max(width[c], (*row)[c].size());
    }

    std::string out;
    auto emitRow = [&](const std::vector<std::string>& cells) {
        std::string line;
        for (std::size_t c = 0; c < columns; ++c) {
            if (c != 0) line += " | ";
            const std::string_view cell = c < cells.size() ? std::string_view{cells[c]} : std::string_view{};
            if (alignment_[c] == Align::Left) {
                line += std::format("{:<{}}", cell, width[c]);
            } else {
                line += std::format("{:>{}}", cell, width[c]);
            }
        }
        // A left-aligned final column pads out to the column width, leaving invisible
        // trailing spaces that show up in diffs and log greps. Only the padding is dropped;
        // interior alignment is untouched.
        line.erase(line.find_last_not_of(' ') + 1);
        out += line;
        out += '\n';
    };
    auto emitRule = [&]() {
        for (std::size_t c = 0; c < columns; ++c) {
            if (c != 0) out += "-+-";
            out += std::string(width[c], '-');
        }
        out += '\n';
    };

    emitRow(headers_);
    emitRule();
    for (const auto& row : rows_) {
        if (row) {
            emitRow(*row);
        } else {
            emitRule();
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Series rendering
// ---------------------------------------------------------------------------

std::string renderSeries(std::string_view identity, std::span<const Timestamp> timestamps,
                         std::span<const double> values, const FormatSpec& spec) {
    std::string out{identity};
    out += '\n';

    const std::size_t n = values.size();
    if (n == 0) return out;

    // How many rows at each end, and whether an ellipsis separates them.
    std::size_t headRows = 0;
    std::size_t tailRows = 0;
    switch (spec.mode) {
        case FormatMode::Head:
            headRows = std::min(spec.count, n);
            break;
        case FormatMode::Tail:
            tailRows = std::min(spec.count, n);
            break;
        default:
            if (n <= spec.count) {
                headRows = n;
            } else {
                const std::size_t half = std::max<std::size_t>(spec.count / 2, 1);
                headRows = half;
                tailRows = half;
            }
            break;
    }

    const auto value = columnFormat(values, spec.precision);
    const bool dated = timestamps.size() >= n;

    Table table(dated ? std::vector<std::string>{"index", "date", "value"} : std::vector<std::string>{"index", "value"},
                dated ? std::vector<Table::Align>{Table::Align::Right, Table::Align::Left, Table::Align::Right}
                      : std::vector<Table::Align>{Table::Align::Right, Table::Align::Right});

    auto emit = [&](std::size_t i) {
        if (dated) {
            table.addRow({std::format("{}", i), common::utils::time::msToStringDate(timestamps[i]), value(values[i])});
        } else {
            table.addRow({std::format("{}", i), value(values[i])});
        }
    };

    for (std::size_t i = 0; i < headRows; ++i) emit(i);
    if (headRows + tailRows < n) {
        table.addRow(dated ? std::vector<std::string>{"...", "...", "..."} : std::vector<std::string>{"...", "..."});
    }
    for (std::size_t i = n - tailRows; i < n; ++i) emit(i);

    out += table.render();
    if (headRows + tailRows < n) out += std::format("[{} rows, {} shown]\n", n, headRows + tailRows);
    return out;
}

std::string renderDescribe(std::string_view identity, std::span<const double> values, int precision) {
    std::string out{identity};
    out += '\n';

    std::vector<double> finite;
    finite.reserve(values.size());
    for (double v : values) {
        if (std::isfinite(v)) finite.push_back(v);
    }

    Table table({"statistic", "value"}, {Table::Align::Left, Table::Align::Right});
    table.addRow({"count", std::format("{}", values.size())});
    if (finite.size() != values.size()) {
        table.addRow({"non-finite", std::format("{}", values.size() - finite.size())});
    }

    if (finite.empty()) {
        out += table.render();
        return out;
    }

    std::sort(finite.begin(), finite.end());
    const auto value = columnFormat(finite, precision);

    table.addRow({"mean", value(analysis::stats::mean(finite))});
    table.addRow({"std",
                  finite.size() < 2
                      ? "N/A"
                      : value(analysis::stats::standardDeviation(finite, analysis::stats::VarianceType::Sample))});
    table.addRow({"min", value(finite.front())});
    table.addRow({"25%", value(analysis::stats::quantileSorted(finite, 0.25))});
    table.addRow({"50%", value(analysis::stats::quantileSorted(finite, 0.50))});
    table.addRow({"75%", value(analysis::stats::quantileSorted(finite, 0.75))});
    table.addRow({"max", value(finite.back())});

    out += table.render();
    return out;
}

}  // namespace ts::fmt

// ---------------------------------------------------------------------------
// std::formatter specializations
// ---------------------------------------------------------------------------

auto std::formatter<ts::fmt::AsDate>::format(const ts::fmt::AsDate& d, std::format_context& ctx) const
    -> std::format_context::iterator {
    return std::format_to(ctx.out(), "{}", ts::common::utils::time::msToStringDate(d.ms));
}

auto std::formatter<ts::fmt::AsDateTime>::format(const ts::fmt::AsDateTime& d, std::format_context& ctx) const
    -> std::format_context::iterator {
    return std::format_to(ctx.out(), "{}", ts::common::utils::time::msToStringISO8601(d.ms));
}
