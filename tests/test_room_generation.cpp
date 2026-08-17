#include "test_harness.hpp"

#include "game/rooms/RoomGroup.hpp"

TEST_CASE("house zones give each wing a spatial identity")
{
    CHECK(RoomGroup::zoneForOffset(0, 0) == RoomGroup::Zone::HEART);
    CHECK(RoomGroup::zoneForOffset(1, 1) == RoomGroup::Zone::HEART);
    CHECK(RoomGroup::zoneForOffset(-4, 1) == RoomGroup::Zone::SERVICE);
    CHECK(RoomGroup::zoneForOffset(4, -1) == RoomGroup::Zone::QUARTERS);
    CHECK(RoomGroup::zoneForOffset(1, 5) == RoomGroup::Zone::CELLAR);
}

TEST_CASE("room pools are themed and deterministic")
{
    CHECK_EQ(RoomGroup::roomTypeFor(RoomGroup::Zone::HEART, 0), 2);
    CHECK_EQ(RoomGroup::roomTypeFor(RoomGroup::Zone::SERVICE, 1), 6);
    CHECK_EQ(RoomGroup::roomTypeFor(RoomGroup::Zone::QUARTERS, 2), 12);
    CHECK_EQ(RoomGroup::roomTypeFor(RoomGroup::Zone::CELLAR, 2), 9);
    CHECK_EQ(RoomGroup::roomTypeFor(RoomGroup::Zone::HEART, 4), 2);
}

TEST_MAIN()