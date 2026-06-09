///////////////
// Random.hpp
//
// Pseudo-random number generator API. The implementation is the
// Park-Miller multiplicative congruential generator (originally
// "appropriated" from a simulations class). Only the symbols that
// the game actually consumes are exposed.
//
///////////////
#ifndef SEEDY_RANDOM_HPP
#define SEEDY_RANDOM_HPP

// Returns a uniform-[0.0, 1.0) double from the current stream.
double Random(void);
// Seeds every stream from a master seed. Pass a positive number for a
// deterministic run, or any negative number to seed from the system clock.
void PlantSeeds(long x);
// Switches the current stream that Random() draws from (0..255).
void SelectStream(int index);

// Returns a uniformly distributed integer in [a, b] inclusive (a < b).
long Equilikely(long a, long b);

#endif
