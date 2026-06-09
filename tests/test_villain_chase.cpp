// Villain "gravitate toward closest player" tests.
//
// What this exercises:
//   * nearestTargetCharacter() picks the closest eligible Character by
//     squared distance (closest of multiple).
//   * Dead, invulnerable, and stationary-SIS characters are skipped so
//     the villain can't lock onto them.
//   * The villain itself is never chosen as its own target.
//   * When nobody is eligible (everyone dead / only the villain in the
//     group), the helper returns nullptr instead of crashing.
//
// These tests bypass Character::init() / Villain::init() so they don't
// need any sprite, sound, or font resources -- only the bookkeeping
// fields (position, health, character, invul, direction) matter.

#include "test_harness.hpp"
#include "components/EntityGroup.hpp"
#include "game/characters/Character.hpp"
#include "game/characters/Villain.hpp"
#include "game/objects/Clue.hpp"
#include "game/Config.hpp"

#include <memory>

namespace {

std::shared_ptr<Character> makePlayer(int slot,
                                      Config::CHARACTER c,
                                      float x, float y,
                                      int hp = 3,
                                      bool invul = false,
                                      sf::Vector2f dir = sf::Vector2f(1.f, 0.f))
{
    auto p = std::make_shared<Character>();
    p->setPlayerNumber(slot);
    p->setCharacter(c);
    p->health    = hp;
    p->maxHealth = 3;
    p->invul     = invul;
    p->direction = dir;
    p->setPosition(x, y);
    return p;
}

std::shared_ptr<Villain> makeVillain(EntityGroup* g, float x, float y)
{
    auto v = std::make_shared<Villain>();
    v->setEntities(g);
    v->setPosition(x, y);
    return v;
}

} // namespace

TEST_CASE("nearestTargetCharacter returns the closest live player by distance")
{
    EntityGroup g;
    auto far  = makePlayer(1, Config::CHARACTER::BRO, 1000.f, 0.f);
    auto near = makePlayer(2, Config::CHARACTER::DAD,  100.f, 0.f);
    auto v    = makeVillain(&g, 0.f, 0.f);
    g.addCharacter(far);
    g.addCharacter(near);
    g.addCharacter(v);

    Character* picked = v->nearestTargetCharacter();
    REQUIRE(picked != nullptr);
    CHECK_EQ(picked->player_number, 2);
}

TEST_CASE("nearestTargetCharacter skips dead characters")
{
    EntityGroup g;
    auto dead  = makePlayer(1, Config::CHARACTER::BRO,  10.f, 0.f, /*hp=*/0);
    auto alive = makePlayer(2, Config::CHARACTER::DAD, 500.f, 0.f);
    auto v     = makeVillain(&g, 0.f, 0.f);
    g.addCharacter(dead);
    g.addCharacter(alive);
    g.addCharacter(v);

    Character* picked = v->nearestTargetCharacter();
    REQUIRE(picked != nullptr);
    CHECK_EQ(picked->player_number, 2);
}

TEST_CASE("nearestTargetCharacter skips invul characters")
{
    EntityGroup g;
    auto invul = makePlayer(1, Config::CHARACTER::BRO,  10.f, 0.f, 3, /*invul=*/true);
    auto plain = makePlayer(2, Config::CHARACTER::DAD, 500.f, 0.f);
    auto v     = makeVillain(&g, 0.f, 0.f);
    g.addCharacter(invul);
    g.addCharacter(plain);
    g.addCharacter(v);

    Character* picked = v->nearestTargetCharacter();
    REQUIRE(picked != nullptr);
    CHECK_EQ(picked->player_number, 2);
}

TEST_CASE("nearestTargetCharacter skips SIS while she's standing still")
{
    EntityGroup g;
    // SIS sits much closer but is stationary -- she's invisible to
    // the villain by design (stealth mechanic).
    auto sis_still = makePlayer(1, Config::CHARACTER::SIS,
                                10.f, 0.f, 3, false,
                                sf::Vector2f(0.f, 0.f));
    auto mom       = makePlayer(2, Config::CHARACTER::MOM, 500.f, 0.f);
    auto v         = makeVillain(&g, 0.f, 0.f);
    g.addCharacter(sis_still);
    g.addCharacter(mom);
    g.addCharacter(v);

    Character* picked = v->nearestTargetCharacter();
    REQUIRE(picked != nullptr);
    CHECK_EQ(picked->player_number, 2);
}

TEST_CASE("nearestTargetCharacter picks SIS once she starts moving")
{
    EntityGroup g;
    auto sis_moving = makePlayer(1, Config::CHARACTER::SIS,
                                 10.f, 0.f, 3, false,
                                 sf::Vector2f(1.f, 0.f));
    auto mom        = makePlayer(2, Config::CHARACTER::MOM, 500.f, 0.f);
    auto v          = makeVillain(&g, 0.f, 0.f);
    g.addCharacter(sis_moving);
    g.addCharacter(mom);
    g.addCharacter(v);

    Character* picked = v->nearestTargetCharacter();
    REQUIRE(picked != nullptr);
    CHECK_EQ(picked->player_number, 1);
}

TEST_CASE("nearestTargetCharacter never picks the villain itself")
{
    EntityGroup g;
    auto v1 = makeVillain(&g, 0.f, 0.f);
    auto v2 = makeVillain(&g, 50.f, 0.f); // even closer than any player
    auto p  = makePlayer(1, Config::CHARACTER::BRO, 100.f, 0.f);
    g.addCharacter(v1);
    g.addCharacter(v2);
    g.addCharacter(p);

    Character* picked = v1->nearestTargetCharacter();
    REQUIRE(picked != nullptr);
    CHECK_EQ(picked->player_number, 1);
}

TEST_CASE("nearestTargetCharacter returns nullptr when every player is dead")
{
    EntityGroup g;
    auto dead1 = makePlayer(1, Config::CHARACTER::BRO,  10.f, 0.f, /*hp=*/0);
    auto dead2 = makePlayer(2, Config::CHARACTER::DAD, 500.f, 0.f, /*hp=*/0);
    auto v     = makeVillain(&g, 0.f, 0.f);
    g.addCharacter(dead1);
    g.addCharacter(dead2);
    g.addCharacter(v);

    CHECK(v->nearestTargetCharacter() == nullptr);
}

TEST_CASE("nearestTargetCharacter returns nullptr when only the villain is in the group")
{
    EntityGroup g;
    auto v = makeVillain(&g, 0.f, 0.f);
    g.addCharacter(v);

    CHECK(v->nearestTargetCharacter() == nullptr);
}

TEST_CASE("nearestTargetCharacter picks the closest by 2D distance, not just X")
{
    EntityGroup g;
    auto faraway = makePlayer(1, Config::CHARACTER::BRO, 200.f,   0.f);
    auto closer  = makePlayer(2, Config::CHARACTER::DAD,  50.f,  50.f);
    auto v       = makeVillain(&g, 0.f, 0.f);
    g.addCharacter(faraway);
    g.addCharacter(closer);
    g.addCharacter(v);

    Character* picked = v->nearestTargetCharacter();
    REQUIRE(picked != nullptr);
    // dist^2(faraway) = 40000, dist^2(closer) = 5000. Closer wins.
    CHECK_EQ(picked->player_number, 2);
}

TEST_MAIN()
