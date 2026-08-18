#include "test_harness.hpp"

#include "game/SpectatorSupport.hpp"
#include "game/rooms/Room.hpp"

TEST_CASE("spectator warning marks one room for four seconds")
{
    Room marked;
    Room other;
    auto& support = SpectatorSupport::instance();
    support.reset();

    CHECK(support.ping(&marked, 3));
    CHECK(support.activeIn(&marked));
    CHECK(!support.activeIn(&other));
    CHECK_EQ(support.signaledBy(), 3);

    support.update(4.f);
    CHECK(!support.activeIn(&marked));
}

TEST_CASE("spectator warning respects its cooldown")
{
    Room room;
    auto& support = SpectatorSupport::instance();
    support.reset();

    REQUIRE(support.ping(&room, 2));
    CHECK(!support.ping(&room, 2));
    support.update(12.f);
    CHECK(support.ping(&room, 2));
}

TEST_MAIN()