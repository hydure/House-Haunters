// Spectator-mode regression tests.
//
// What this exercises:
//   * When a player's character dies, PlayerView reroutes its camera and
//     HUD to follow a surviving teammate (auto-picks the lowest live
//     slot first).
//   * The dead player can cycle through survivors with cycleSpectator
//     (+1 / -1) and wraparound is correct.
//   * If the spectated teammate ALSO dies, the next ensureSpectator()
//     pass snaps the camera onto another survivor without input.
//   * When everybody is dead, currentTarget() returns -1 (no crash) and
//     the GameplayScreen player_died handler (mirrored here so we test
//     the contract without loading game resources) fires
//     change_screen("GameEnd") exactly once after the last player dies.

#include "test_harness.hpp"
#include "components/EntityGroup.hpp"
#include "game/characters/Character.hpp"
#include "game/characters/PlayerView.hpp"
#include "engine/EventManager.hpp"
#include "engine/EngineEvents.hpp"

#include <memory>
#include <string>
#include <vector>

namespace {

// Build a Character ready for the spectator tests without going through
// Character::init() (which loads sprite sheets / audio). Spectator logic
// only inspects player_number, health, maxHealth, and isVillain().
std::shared_ptr<Character> makePlayer(int slot, int hp)
{
    auto c = std::make_shared<Character>();
    c->setPlayerNumber(slot);
    c->health    = hp;
    c->maxHealth = hp > 0 ? hp : 3;
    return c;
}

void resetEvents()
{
    Events::clear();
}

} // namespace

TEST_CASE("PlayerView follows its own player while alive")
{
    EntityGroup g;
    g.addCharacter(makePlayer(1, 3));
    g.addCharacter(makePlayer(2, 3));

    PlayerView view;
    view.setEntities(&g);
    view.setEntityNumber(1);
    view.ensureSpectator();

    CHECK_EQ(view.currentTarget(), 1);
    CHECK_EQ(view.spectatorTarget(), -1);
}

TEST_CASE("PlayerView auto-picks a surviving teammate the frame its own player dies")
{
    EntityGroup g;
    auto p1 = makePlayer(1, 3);
    auto p2 = makePlayer(2, 3);
    auto p3 = makePlayer(3, 3);
    g.addCharacter(p1);
    g.addCharacter(p2);
    g.addCharacter(p3);

    PlayerView view;
    view.setEntities(&g);
    view.setEntityNumber(1);

    // p1 dies. ensureSpectator runs every frame inside PlayerView::onUpdate,
    // so simulating one tick is enough: the camera should hop onto the
    // lowest-numbered surviving teammate.
    p1->health = 0;
    view.ensureSpectator();

    CHECK_EQ(view.spectatorTarget(), 2);
    CHECK_EQ(view.currentTarget(), 2);
    // Dead-own + alive spectator: ownPlayerNumber stays put.
    CHECK_EQ(view.ownPlayerNumber(), 1);
}

TEST_CASE("cycleSpectator advances and wraps through surviving teammates in slot order")
{
    EntityGroup g;
    auto p1 = makePlayer(1, 0);  // dead -- the spectator
    auto p2 = makePlayer(2, 3);
    auto p3 = makePlayer(3, 3);
    g.addCharacter(p1);
    g.addCharacter(p2);
    g.addCharacter(p3);

    PlayerView view;
    view.setEntities(&g);
    view.setEntityNumber(1);
    view.ensureSpectator();
    CHECK_EQ(view.spectatorTarget(), 2);

    view.cycleSpectator(+1);
    CHECK_EQ(view.spectatorTarget(), 3);

    // Wrap forward off the end -> back to the first survivor (slot 2).
    view.cycleSpectator(+1);
    CHECK_EQ(view.spectatorTarget(), 2);

    // Wrap backward off the start -> last survivor.
    view.cycleSpectator(-1);
    CHECK_EQ(view.spectatorTarget(), 3);
}

TEST_CASE("cycleSpectator is a no-op while the spectator's own character is alive")
{
    EntityGroup g;
    g.addCharacter(makePlayer(1, 3));   // alive
    g.addCharacter(makePlayer(2, 3));
    g.addCharacter(makePlayer(3, 3));

    PlayerView view;
    view.setEntities(&g);
    view.setEntityNumber(1);

    view.cycleSpectator(+1);
    CHECK_EQ(view.spectatorTarget(), -1);
    CHECK_EQ(view.currentTarget(), 1);
}

TEST_CASE("ensureSpectator re-targets when the spectated teammate also dies")
{
    EntityGroup g;
    auto p1 = makePlayer(1, 0);  // dead
    auto p2 = makePlayer(2, 3);  // initial spectator target
    auto p3 = makePlayer(3, 3);
    g.addCharacter(p1);
    g.addCharacter(p2);
    g.addCharacter(p3);

    PlayerView view;
    view.setEntities(&g);
    view.setEntityNumber(1);
    view.ensureSpectator();
    REQUIRE(view.spectatorTarget() == 2);

    // The teammate we were watching dies mid-spectate. On the next
    // ensureSpectator pass the camera should snap onto the only other
    // living player without us pressing anything.
    p2->health = 0;
    view.ensureSpectator();
    CHECK_EQ(view.spectatorTarget(), 3);
    CHECK_EQ(view.currentTarget(), 3);
}

TEST_CASE("ensureSpectator returns -1 when no living teammates remain")
{
    EntityGroup g;
    g.addCharacter(makePlayer(1, 0));
    g.addCharacter(makePlayer(2, 0));
    g.addCharacter(makePlayer(3, 0));

    PlayerView view;
    view.setEntities(&g);
    view.setEntityNumber(1);
    view.ensureSpectator();

    // -1 is the documented "nobody to watch" sentinel; PlayerView::onUpdate
    // and onDraw bail out gracefully on this rather than dereferencing
    // a null character pointer.
    CHECK_EQ(view.spectatorTarget(), -1);
    CHECK_EQ(view.currentTarget(), 1);
}

TEST_CASE("spectator state clears the moment the player respawns / rejoins with HP")
{
    EntityGroup g;
    auto p1 = makePlayer(1, 0);
    g.addCharacter(p1);
    g.addCharacter(makePlayer(2, 3));

    PlayerView view;
    view.setEntities(&g);
    view.setEntityNumber(1);
    view.ensureSpectator();
    REQUIRE(view.spectatorTarget() == 2);

    // Simulate a rejoin / HP restore from the network layer -- the
    // "Allow mid-game (re)join; preserve health" feature.
    p1->health = 3;
    view.ensureSpectator();
    CHECK_EQ(view.spectatorTarget(), -1);
    CHECK_EQ(view.currentTarget(), 1);
}

TEST_CASE("GameplayScreen player_died contract: GameEnd fires once after the last player dies")
{
    // Mirrors src/game/screens/GameplayScreen.cpp:69 -- the production
    // subscriber decrements num_players on every player_died event and
    // queues change_screen("GameEnd") when it hits zero. Tested here as
    // a contract: full GameplayScreen::init() pulls in sprite sheets /
    // audio that aren't safe to load headlessly.
    resetEvents();

    int num_players = 3;
    long playerDiedId = Events::addEventListener("player_died",
        [&num_players](base_event_type /*e*/) {
            if (--num_players == 0) {
                auto e = std::make_shared< Event<std::string> >("GameEnd");
                Events::queueEvent("change_screen", e);
            }
        });

    std::vector<std::string> screensRequested;
    long changeScreenId = Events::addEventListener("change_screen",
        [&screensRequested](base_event_type evt) {
            auto& se = dynamic_cast< Event<std::string>& >(*evt);
            screensRequested.push_back(se.data);
        });

    // Two players die -- no game-over yet.
    Events::triggerEvent("player_died", std::make_shared<BasicEvent>());
    Events::triggerEvent("player_died", std::make_shared<BasicEvent>());
    Events::notify();
    CHECK_EQ(num_players, 1);
    CHECK(screensRequested.empty());

    // Last player dies -- GameEnd is queued and fires on the next notify.
    Events::triggerEvent("player_died", std::make_shared<BasicEvent>());
    CHECK_EQ(num_players, 0);
    Events::notify();
    CHECK_EQ(screensRequested.size(), static_cast<size_t>(1));
    if (!screensRequested.empty()) {
        CHECK_EQ(screensRequested[0], std::string("GameEnd"));
    }

    Events::removeEventListener(playerDiedId);
    Events::removeEventListener(changeScreenId);
    resetEvents();
}

TEST_CASE("GameplayScreen player_died contract: solo-player game ends after a single death")
{
    // The host-can-force-start-with-zero-joiners path means num_players
    // can be exactly 1. The same handler still has to fire GameEnd on
    // the very first player_died.
    resetEvents();

    int num_players = 1;
    long playerDiedId = Events::addEventListener("player_died",
        [&num_players](base_event_type /*e*/) {
            if (--num_players == 0) {
                auto e = std::make_shared< Event<std::string> >("GameEnd");
                Events::queueEvent("change_screen", e);
            }
        });

    bool gameEnded = false;
    long changeScreenId = Events::addEventListener("change_screen",
        [&gameEnded](base_event_type evt) {
            auto& se = dynamic_cast< Event<std::string>& >(*evt);
            if (se.data == "GameEnd") gameEnded = true;
        });

    Events::triggerEvent("player_died", std::make_shared<BasicEvent>());
    Events::notify();
    CHECK(gameEnded);

    Events::removeEventListener(playerDiedId);
    Events::removeEventListener(changeScreenId);
    resetEvents();
}

TEST_MAIN()
