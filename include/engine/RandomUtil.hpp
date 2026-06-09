#ifndef RANDOM_UTIL_HPP
#define RANDOM_UTIL_HPP

#include "engine/Random.hpp"

////////////////////////////////
// RandomUtil.hpp
//
// Thin convenience wrappers around the Park & Geyer Random module
// (engine/Random.hpp) for the integer cases we used to write with
// `rand() % n`. Keeping a single entry point makes it possible to seed
// the generator deterministically (PlantSeeds) for replays and tests.
////////////////////////////////

// Returns a uniform integer in the half-open range [0, n). Returns 0 if
// n <= 0 so callers don't have to special-case empty containers.
inline int randomInt(int n)
{
    if (n <= 1) {
        return 0;
    }
    return static_cast<int>(Equilikely(0, n - 1));
}

// Returns a uniform integer in the closed range [lo, hi]. If hi < lo,
// returns lo (defensive; callers shouldn't pass an inverted range).
inline int randomIntRange(int lo, int hi)
{
    if (hi <= lo) {
        return lo;
    }
    return static_cast<int>(Equilikely(lo, hi));
}

#endif
