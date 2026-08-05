// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

// Eigen ships no std::formatter, so Eigen operands cannot appear in an ensure() message
// or a log line. This header supplies one for every dense Eigen expression.
//
// Kept out of Format.hpp on purpose: Format.hpp is included by TimeSeries.hpp, which has
// no other reason to pull in Eigen. Include this only where a matrix is actually printed.

#include <Eigen/Core>
#include <concepts>
#include <cstddef>
#include <format>
#include <string>
#include <vector>

#include "finlib/common/Format.hpp"

namespace ts::fmt {

// Row-major dump of any dense Eigen expression, columns aligned on a shared precision.
// `label` prefixes the shape line; empty for none.
template <class Derived>
std::string renderMatrix(const Eigen::DenseBase<Derived>& m, int precision = -1, std::string_view label = {}) {
    const auto rows = static_cast<std::size_t>(m.rows());
    const auto cols = static_cast<std::size_t>(m.cols());

    std::string out;
    if (!label.empty()) out += std::format("{} ", label);
    out += std::format("[{}x{}]\n", rows, cols);
    if (rows == 0 || cols == 0) return out;

    std::vector<double> flat;
    flat.reserve(rows * cols);
    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            flat.push_back(static_cast<double>(m(static_cast<Eigen::Index>(r), static_cast<Eigen::Index>(c))));
        }
    }
    const auto value = columnFormat(flat, precision);

    std::vector<std::string> headers{""};
    std::vector<Table::Align> alignment{Table::Align::Right};
    for (std::size_t c = 0; c < cols; ++c) {
        headers.push_back(std::format("{}", c));
        alignment.push_back(Table::Align::Right);
    }

    Table table(std::move(headers), std::move(alignment));
    for (std::size_t r = 0; r < rows; ++r) {
        std::vector<std::string> row{std::format("{}", r)};
        for (std::size_t c = 0; c < cols; ++c) row.push_back(value(flat[r * cols + c]));
        table.addRow(std::move(row));
    }
    out += table.render();
    return out;
}

}  // namespace ts::fmt

// Constrained on Eigen's CRTP base so this claims Eigen's dense types and nothing else.
// `{:.4}` pins the decimals; the plain form derives them from the entries.
template <class T>
    requires std::derived_from<T, Eigen::DenseBase<T>>
struct std::formatter<T> {
    ts::fmt::FormatSpec spec;

    constexpr auto parse(std::format_parse_context& ctx) { return ts::fmt::parseFormatSpec(ctx, spec); }

    auto format(const T& m, std::format_context& ctx) const -> std::format_context::iterator {
        return std::format_to(ctx.out(), "{}", ts::fmt::renderMatrix(m, spec.precision));
    }
};
