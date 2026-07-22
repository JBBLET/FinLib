// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace finance {
using Timestamp = int64_t;
using TimestampsPtr = std::shared_ptr<std::vector<int64_t>>;

// Set operations over sorted, duplicate-free timestamp grids. Both inputs must be
// ascending; the result is ascending and duplicate-free. Used to assemble a portfolio's
// analysis grid from its constituents' native observation ticks.

inline TimestampsPtr unionPair(const TimestampsPtr& gridA, const TimestampsPtr& gridB) {
    const auto& a = *gridA;
    const auto& b = *gridB;
    auto output = std::make_shared<std::vector<Timestamp>>();
    output->reserve(a.size() + b.size());
    size_t i{0}, j{0};
    while (i < a.size() && j < b.size()) {
        if (a[i] < b[j]) {
            output->push_back(a[i++]);
        } else if (b[j] < a[i]) {
            output->push_back(b[j++]);
        } else {
            output->push_back(a[i]);
            ++i;
            ++j;
        }
    }
    while (i < a.size()) output->push_back(a[i++]);
    while (j < b.size()) output->push_back(b[j++]);
    return output;
}

inline TimestampsPtr intersectionPair(const TimestampsPtr& gridA, const TimestampsPtr& gridB) {
    const auto& a = *gridA;
    const auto& b = *gridB;
    auto output = std::make_shared<std::vector<Timestamp>>();
    output->reserve(std::min(a.size(), b.size()));
    size_t i{0}, j{0};
    while (i < a.size() && j < b.size()) {
        if (a[i] < b[j]) {
            ++i;
        } else if (b[j] < a[i]) {
            ++j;
        } else {
            output->push_back(a[i]);
            ++i;
            ++j;
        }
    }
    return output;
}

inline TimestampsPtr unionOf(std::span<const TimestampsPtr> grids) {
    if (grids.empty()) return std::make_shared<std::vector<Timestamp>>();
    TimestampsPtr output = grids[0];
    for (size_t i = 1; i < grids.size(); ++i) output = unionPair(output, grids[i]);
    return output;
}

inline TimestampsPtr intersectionOf(std::span<const TimestampsPtr> grids) {
    if (grids.empty()) return std::make_shared<std::vector<Timestamp>>();
    TimestampsPtr output = grids[0];
    for (size_t i = 1; i < grids.size(); ++i) output = intersectionPair(output, grids[i]);
    return output;
}
}  // namespace finance
