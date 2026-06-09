// CharacterScreen lobby-gate + hot-plug tests.
//
// What this exercises:
//   * When LOCAL PLAY enters with an empty player_map, the screen sits
//     in an "awaiting P1" gate (player count 0, gate prompt visible).
//   * The first non-MENU released button on ANY controller drops the
//     gate and claims that controller index as P1 in config->player_map.
//   * MENU during the gate routes back to the Title screen and DOES
//     NOT claim a player slot.
//   * After the gate has been cleared, a gamepad_connect event for a
//     fresh controller index auto-adds that controller as the next
//     player slot (P2 / P3 / P4) without waiting for the controller
//     itself to press a button.
//   * gamepad_connect while the gate is still up does NOT pre-seed
//     player slots -- the host still gets to decide who is P1.
//   * The legacy entry path (player_map already seeded by a network
//     screen or a test) does NOT enter the gate.

#include "test_harness.hpp"
#include "engine/EventManager.hpp"
#include "engine/EngineEvents.hpp"
#include "engine/Gamepad.hpp"
#include "game/Config.hpp"
#include "game/screens/CharacterScreen.hpp"

#include <memory>
#include <string>
#include <vector>

namespace {

void resetEvents()
{
    Events::clear();
}

GamepadEvent makeReleased(const std::string& button, int index = 0)
{
    GamepadEvent e;
    e.button = button;
    e.type   = GamepadEvent::RELEASED;
    e.index  = index;
    return e;
}

GamepadEvent makeConnect(int index)
{
    GamepadEvent e;
    e.button = "";
    e.type   = GamepadEvent::CONNECT;
    e.index  = index;
    return e;
}

struct ChangeScreenCapture {
    long id = -1;
    std::vector<std::string> targets;
    ChangeScreenCapture() {
        id = Events::addEventListener("change_screen",
            [this](base_event_type evt){
                auto& se = dynamic_cast< Event<std::string>& >(*evt);
                targets.push_back(se.data);
            });
    }
    ~ChangeScreenCapture() {
        if (id >= 0) Events::removeEventListener(id);
    }
};

} // namespace

TEST_CASE("CharacterScreen enters the awaiting-P1 gate when player_map is empty")
{
    resetEvents();
    CharacterScreen chars;
    auto cfg = std::make_shared<Config>();
    chars.setConfig(cfg);
    chars.init();

    CHECK(chars.isAwaitingFirstPlayer());
    CHECK_EQ(chars.playerCount(), 0);
    CHECK_EQ(cfg->player_map.size(), static_cast<size_t>(0));
}

TEST_CASE("First non-MENU release claims P1 and drops the gate")
{
    resetEvents();
    CharacterScreen chars;
    auto cfg = std::make_shared<Config>();
    chars.setConfig(cfg);
    chars.init();
    REQUIRE(chars.isAwaitingFirstPlayer());

    // Player on controller index 2 presses A. They should be promoted
    // to P1 and the gate should clear.
    auto evt = std::make_shared<GamepadEvent>(makeReleased("A", 2));
    Events::triggerEvent("gamepad_event", evt);

    CHECK(!chars.isAwaitingFirstPlayer());
    CHECK_EQ(chars.playerCount(), 1);
    CHECK_EQ(cfg->player_map.count(2), static_cast<size_t>(1));
    CHECK_EQ(cfg->player_map[2], 1);
}

TEST_CASE("MENU during the gate routes to Title without claiming P1")
{
    resetEvents();
    CharacterScreen chars;
    auto cfg = std::make_shared<Config>();
    chars.setConfig(cfg);
    chars.init();
    REQUIRE(chars.isAwaitingFirstPlayer());

    ChangeScreenCapture cap;

    auto evt = std::make_shared<GamepadEvent>(makeReleased("MENU", 0));
    Events::triggerEvent("gamepad_event", evt);

    bool sawTitle = false;
    for (const auto& t : cap.targets) if (t == "Title") sawTitle = true;
    CHECK(sawTitle);
    // Crucially, nobody was registered as a player.
    CHECK(chars.isAwaitingFirstPlayer());
    CHECK_EQ(chars.playerCount(), 0);
    CHECK_EQ(cfg->player_map.size(), static_cast<size_t>(0));
}

TEST_CASE("gamepad_connect during the gate does NOT pre-seed a player")
{
    resetEvents();
    CharacterScreen chars;
    auto cfg = std::make_shared<Config>();
    chars.setConfig(cfg);
    chars.init();
    REQUIRE(chars.isAwaitingFirstPlayer());

    // A second controller hot-plugs in before P1 has decided to start.
    // The screen should NOT silently register them -- the host is the
    // one who gets to drop the gate.
    auto evt = std::make_shared<GamepadEvent>(makeConnect(1));
    Events::triggerEvent("gamepad_connect", evt);

    CHECK(chars.isAwaitingFirstPlayer());
    CHECK_EQ(chars.playerCount(), 0);
    CHECK_EQ(cfg->player_map.size(), static_cast<size_t>(0));
}

TEST_CASE("gamepad_connect after the gate auto-adds the next player slot")
{
    resetEvents();
    CharacterScreen chars;
    auto cfg = std::make_shared<Config>();
    chars.setConfig(cfg);
    chars.init();

    // Drop the gate so we're in the "lobby is active" phase.
    auto p1 = std::make_shared<GamepadEvent>(makeReleased("A", 0));
    Events::triggerEvent("gamepad_event", p1);
    REQUIRE(!chars.isAwaitingFirstPlayer());
    REQUIRE(chars.playerCount() == 1);

    // Controller index 3 plugs in. The screen should auto-add them as
    // P2 without waiting for them to press a button.
    auto plug = std::make_shared<GamepadEvent>(makeConnect(3));
    Events::triggerEvent("gamepad_connect", plug);

    CHECK_EQ(chars.playerCount(), 2);
    CHECK_EQ(cfg->player_map.count(3), static_cast<size_t>(1));
    CHECK_EQ(cfg->player_map[3], 2);
}

TEST_CASE("gamepad_connect for an already-registered controller is a no-op")
{
    resetEvents();
    CharacterScreen chars;
    auto cfg = std::make_shared<Config>();
    chars.setConfig(cfg);
    chars.init();

    // Drop the gate.
    auto p1 = std::make_shared<GamepadEvent>(makeReleased("A", 0));
    Events::triggerEvent("gamepad_event", p1);
    REQUIRE(chars.playerCount() == 1);

    // The SAME controller "re-connects" (e.g. a debounce blip). The
    // player count must not double-count them.
    auto plug = std::make_shared<GamepadEvent>(makeConnect(0));
    Events::triggerEvent("gamepad_connect", plug);

    CHECK_EQ(chars.playerCount(), 1);
    CHECK_EQ(cfg->player_map.size(), static_cast<size_t>(1));
}

TEST_CASE("gamepad_connect caps the lobby at four players")
{
    resetEvents();
    CharacterScreen chars;
    auto cfg = std::make_shared<Config>();
    chars.setConfig(cfg);
    chars.init();

    // Drop the gate then fill the lobby with three more controllers.
    auto fire = [](GamepadEvent ev, const std::string& topic) {
        Events::triggerEvent(topic, std::make_shared<GamepadEvent>(ev));
    };
    fire(makeReleased("A", 0), "gamepad_event");
    fire(makeConnect(1),       "gamepad_connect");
    fire(makeConnect(2),       "gamepad_connect");
    fire(makeConnect(3),       "gamepad_connect");
    REQUIRE(chars.playerCount() == 4);

    // A fifth hot-plug must NOT add another player.
    fire(makeConnect(4), "gamepad_connect");

    CHECK_EQ(chars.playerCount(), 4);
    CHECK_EQ(cfg->player_map.count(4), static_cast<size_t>(0));
}

TEST_CASE("Pre-seeded player_map enters the lobby without the gate")
{
    // Legacy path: HostScreen and the existing screen-flow tests still
    // seed player_map themselves. Those entries must come up in the
    // "lobby already active" state, not the gate.
    resetEvents();
    CharacterScreen chars;
    auto cfg = std::make_shared<Config>();
    cfg->player_map[0] = 1;
    chars.setConfig(cfg);
    chars.init();

    CHECK(!chars.isAwaitingFirstPlayer());
    CHECK_EQ(chars.playerCount(), 1);
}

TEST_MAIN()
