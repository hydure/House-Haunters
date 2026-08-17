#include "test_harness.hpp"

#include <memory>

#include "components/EntityGroup.hpp"
#include "game/characters/Character.hpp"
#include "game/characters/VillainDirector.hpp"
#include "game/objects/Clue.hpp"
#include "game/rooms/Room.hpp"

namespace {
std::shared_ptr<Character> makeFamily(EntityGroup* group,
                                      int slot,
                                      Config::CHARACTER role,
                                      Room* room)
{
    auto character = std::make_shared<Character>();
    character->setEntities(group);
    character->setPlayerNumber(slot);
    character->setCharacter(role);
    character->health = 3;
    character->maxHealth = 3;
    character->currentRoom = room;
    return character;
}
}

TEST_CASE("BRO sprint noise takes Director priority")
{
    EntityGroup group;
    Room room;
    auto bro = makeFamily(&group, 1, Config::BRO, &room);
    auto closerMom = makeFamily(&group, 2, Config::MOM, &room);
    bro->setPosition(500.f, 0.f);
    closerMom->setPosition(10.f, 0.f);
    bro->setDrawingAggro(true);
    group.addCharacter(bro);
    group.addCharacter(closerMom);

    VillainDirector director;
    director.setEntities(&group);
    CHECK(director.nearestTo(sf::Vector2f(0.f, 0.f)) == bro.get());
}

TEST_CASE("DAD absorbs one same-room hit before protection cools down")
{
    EntityGroup group;
    Room room;
    auto mom = makeFamily(&group, 1, Config::MOM, &room);
    auto dad = makeFamily(&group, 2, Config::DAD, &room);
    group.addCharacter(mom);
    group.addCharacter(dad);

    CHECK(mom->hasLivingTeammateInRoom());
    CHECK(mom->redirectHitToDad());
    CHECK_EQ(mom->health, 3);
    CHECK_EQ(dad->health, 2);
    CHECK(!mom->redirectHitToDad());
}

TEST_MAIN()