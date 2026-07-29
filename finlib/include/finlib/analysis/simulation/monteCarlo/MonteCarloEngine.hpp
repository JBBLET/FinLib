// Copyright (c) 2026 JBBLET. All Rights Reserved.

#pragma once
#include <cstddef>
#include <vector>

#include "finlib/common/Random.hpp"

namespace ts::simulation {

struct MonteCarloSpecification {
    std::size_t paths = 0;
    std::size_t steps = 0;
    Seed seed = kDefaultSeed;
};

// A path owns everything about one trajectory: its state, its parameters, and whatever it chose to
// accumulate. The engine only knows how to advance it and how to ask for the answer.
template <class P>
concept PathSimulation = requires(P p, std::size_t t, Rng& g) {
    p.step(t, g);  // advance one step; t runs 1..steps
    p.result();    // whatever the caller wants collected
};

template <class MakePath>
auto run(const MonteCarloSpecification& spec, MakePath makePath)
    -> std::vector<decltype(makePath(std::size_t{}).result())> {
    using Path = decltype(makePath(std::size_t{}));
    using Result = decltype(makePath(std::size_t{}).result());
    static_assert(PathSimulation<Path>, "makePath must return a type with step(size_t, Rng&) and result()");

    std::vector<Result> out;
    out.reserve(spec.paths);
    for (std::size_t p = 0; p < spec.paths; ++p) {
        Rng rng = rngForStream(spec.seed, RngDomain::Simulation, p);
        auto path = makePath(p);
        for (std::size_t t = 1; t <= spec.steps; ++t) path.step(t, rng);
        out.push_back(path.result());
    }
    return out;
}
}  // namespace ts::simulation
