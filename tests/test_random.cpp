// Random number generator + RandomUtil wrappers.
//
// Focuses on the contract relied on elsewhere in the code base:
//   * PlantSeeds(s) makes the stream deterministic (item #4 / #15 rely
//     on this -- two peers seeded the same way must agree).
//   * randomInt(n) is half-open [0, n), defends against n <= 0/1.
//   * randomIntRange(lo, hi) is closed and clamps inverted ranges.

#include "test_harness.hpp"
#include "engine/Random.hpp"
#include "engine/RandomUtil.hpp"

#include <vector>

TEST_CASE("PlantSeeds: same seed -> identical sequence")
{
    PlantSeeds(12345);
    std::vector<double> a;
    for (int i = 0; i < 64; ++i) a.push_back(Random());

    PlantSeeds(12345);
    std::vector<double> b;
    for (int i = 0; i < 64; ++i) b.push_back(Random());

    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        CHECK_EQ(a[i], b[i]);
    }
}

TEST_CASE("PlantSeeds: different seeds -> different sequence")
{
    PlantSeeds(1);
    double a = Random();
    PlantSeeds(2);
    double b = Random();
    CHECK(a != b);
}

TEST_CASE("Random: stays in [0, 1)")
{
    PlantSeeds(42);
    for (int i = 0; i < 2048; ++i) {
        double r = Random();
        CHECK(r >= 0.0);
        CHECK(r < 1.0);
    }
}

TEST_CASE("randomInt: bounds [0, n)")
{
    PlantSeeds(7);
    for (int i = 0; i < 2048; ++i) {
        int r = randomInt(10);
        CHECK(r >= 0);
        CHECK(r < 10);
    }
}

TEST_CASE("randomInt: defensive for n <= 1")
{
    CHECK_EQ(randomInt(0), 0);
    CHECK_EQ(randomInt(1), 0);
    CHECK_EQ(randomInt(-5), 0);
}

TEST_CASE("randomIntRange: closed bounds")
{
    PlantSeeds(99);
    for (int i = 0; i < 2048; ++i) {
        int r = randomIntRange(5, 9);
        CHECK(r >= 5);
        CHECK(r <= 9);
    }
}

TEST_CASE("randomIntRange: inverted range clamps to lo")
{
    CHECK_EQ(randomIntRange(7, 3), 7);
    CHECK_EQ(randomIntRange(2, 2), 2); // hi == lo
}

TEST_CASE("randomIntRange: hits both endpoints over a long sequence")
{
    PlantSeeds(2026);
    bool sawLo = false, sawHi = false;
    for (int i = 0; i < 4096 && (!sawLo || !sawHi); ++i) {
        int r = randomIntRange(0, 3);
        if (r == 0) sawLo = true;
        if (r == 3) sawHi = true;
    }
    CHECK(sawLo);
    CHECK(sawHi);
}

TEST_MAIN()
