#pragma once
#include <cstdint>
#include <format>
#include <random>
#include <string_view>

namespace ts {

using Rng = std::mt19937_64;
using Seed = std::uint64_t;

inline constexpr Seed kDefaultSeed = 0x9E3779B97F4A7C15ULL;

enum class RngDomain : std::uint64_t {
    Resampling = 0x52455341ULL,
    Bootstrap = 0x424F4F54ULL,
    Simulation = 0x53494D55ULL,
};

// Reproducibility complaints are always about which stream produced a number, so the domain
// needs a name in a log line rather than a 64-bit tag nobody can decode on sight.
constexpr std::string_view toString(RngDomain domain) {
    switch (domain) {
        case RngDomain::Resampling: return "Resampling";
        case RngDomain::Bootstrap: return "Bootstrap";
        case RngDomain::Simulation: return "Simulation";
    }
    return "<unknown RngDomain>";
}

// An independent, reproducible stream for (seed, domain, index). Stream i is bit-identical
// regardless of how many streams run, in what order, or on how many threads.
// index is a path in Monte-Carlo, a replicate in a bootstrap, a chunk in a parallel resample.
inline Rng rngForStream(Seed baseSeed, RngDomain domain, std::size_t index) {
    const auto d = static_cast<std::uint64_t>(domain);
    std::seed_seq sq{static_cast<std::uint32_t>(baseSeed),
                     static_cast<std::uint32_t>(baseSeed >> 32),
                     static_cast<std::uint32_t>(d),
                     static_cast<std::uint32_t>(d >> 32),
                     static_cast<std::uint32_t>(index),
                     static_cast<std::uint32_t>(index >> 32)};
    return Rng(sq);
}
}  // namespace ts

template <>
struct std::formatter<ts::RngDomain> : std::formatter<std::string_view> {
    auto format(ts::RngDomain domain, std::format_context& ctx) const -> std::format_context::iterator {
        return std::formatter<std::string_view>::format(ts::toString(domain), ctx);
    }
};
