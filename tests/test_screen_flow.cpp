// Screen-flow regression tests.
//
// What this exercises:
//   * GameEngine::changeGameScreen routes by name, ignores unknown ids,
//     and re-runs init() every time a screen is entered.
//   * GameScreen::clearSubscriptions() actually releases the listeners
//     a screen registered via subscribe() so they DO NOT keep firing
//     after the engine has moved on.
//   * Re-entering a previously-visited screen (e.g. canceling out of
//     HostScreen back to GametitleScreen) leaves the new screen fully
//     initialized -- this is the regression for the bug where the
//     title screen rendered blank after canceling Host because the
//     real init() bailed silently on a non-fatal failure.
//   * onExit() returning false vetoes the change.
//
// These tests avoid SFML window / texture / font code by using stub
// GameScreen subclasses. They drive GameEngine::changeGameScreen() and
// the change_screen event listener path directly, the same code paths
// the real screens go through at runtime.

#include "test_harness.hpp"
#include "engine/GameEngine.hpp"
#include "engine/EventManager.hpp"
#include "engine/EngineEvents.hpp"
#include "engine/Gamepad.hpp"
#include "game/Config.hpp"
#include "game/screens/GametitleScreen.hpp"
#include "game/screens/CharacterScreen.hpp"

#include <memory>
#include <string>

namespace {

void resetEvents()
{
    Events::clear();
}

// A bare GameScreen that records lifecycle calls and registers one
// listener through subscribe() so we can verify the unsubscription
// pipeline.
class StubScreen : public GameScreen
{
public:
    int  initCount      = 0;
    int  pingHits       = 0;
    bool blockExit      = false;
    int  exitCallCount  = 0;

    void init() override
    {
        ++initCount;
        // Same RAII subscription pattern as the real screens.
        this->subscribe("ping", [this](base_event_type) {
            ++pingHits;
        });
    }
    bool onExit() override
    {
        ++exitCallCount;
        return !blockExit;
    }
};

// Convenience: install a transient change_screen listener that mimics
// what GameEngine::start() does, so the tests can fire change_screen
// events the same way real screens do.
struct ChangeScreenBridge {
    long id = -1;
    GameEngine* eng = nullptr;
    explicit ChangeScreenBridge(GameEngine& e) : eng(&e) {
        id = Events::addEventListener("change_screen",
            [this](base_event_type evt){
                auto& se = dynamic_cast< Event<std::string>& >(*evt);
                eng->changeGameScreen(se.data);
            });
    }
    ~ChangeScreenBridge() {
        if (id >= 0) Events::removeEventListener(id);
    }
};

} // namespace

TEST_CASE("changeGameScreen runs init() the first time a screen is entered")
{
    resetEvents();
    GameEngine eng;
    auto* a = new StubScreen();
    eng.addGameScreen("A", std::unique_ptr<GameScreen>(a));

    eng.changeGameScreen("A");
    CHECK_EQ(a->initCount, 1);
}

TEST_CASE("changeGameScreen ignores unknown screen ids without crashing")
{
    resetEvents();
    GameEngine eng;
    auto* a = new StubScreen();
    eng.addGameScreen("A", std::unique_ptr<GameScreen>(a));
    eng.changeGameScreen("A");

    // Bogus name should be a quiet no-op; the current scene stays A.
    eng.changeGameScreen("does-not-exist");
    CHECK_EQ(a->initCount, 1);
    // And A's listener should still be wired up.
    Events::triggerEvent("ping", std::make_shared<BasicEvent>());
    CHECK_EQ(a->pingHits, 1);
}

TEST_CASE("re-entering a screen re-runs init() every time")
{
    // This is the direct regression for "canceled HostScreen -> title
    // came back blank". The title's init() must run on every re-entry,
    // not just the first one.
    resetEvents();
    GameEngine eng;
    auto* title = new StubScreen();
    auto* host  = new StubScreen();
    eng.addGameScreen("Title", std::unique_ptr<GameScreen>(title));
    eng.addGameScreen("Host",  std::unique_ptr<GameScreen>(host));

    eng.changeGameScreen("Title");
    eng.changeGameScreen("Host");
    eng.changeGameScreen("Title"); // simulate "Cancel" returning home.

    CHECK_EQ(title->initCount, 2);
    CHECK_EQ(host->initCount,  1);
}

TEST_CASE("clearSubscriptions releases the outgoing screen's listeners")
{
    resetEvents();
    GameEngine eng;
    auto* a = new StubScreen();
    auto* b = new StubScreen();
    eng.addGameScreen("A", std::unique_ptr<GameScreen>(a));
    eng.addGameScreen("B", std::unique_ptr<GameScreen>(b));

    eng.changeGameScreen("A");
    // A's "ping" listener is live.
    Events::triggerEvent("ping", std::make_shared<BasicEvent>());
    CHECK_EQ(a->pingHits, 1);

    eng.changeGameScreen("B");
    // A is gone -> its listener must NOT fire anymore. B re-registers
    // its own "ping" listener inside init().
    Events::triggerEvent("ping", std::make_shared<BasicEvent>());
    CHECK_EQ(a->pingHits, 1); // unchanged
    CHECK_EQ(b->pingHits, 1);
}

TEST_CASE("re-entering a screen does not double-register its listeners")
{
    // If clearSubscriptions failed to run, the title screen would keep
    // accumulating "gamepad_event" listeners every visit, and one input
    // would fire N copies of every action.
    resetEvents();
    GameEngine eng;
    auto* title = new StubScreen();
    auto* host  = new StubScreen();
    eng.addGameScreen("Title", std::unique_ptr<GameScreen>(title));
    eng.addGameScreen("Host",  std::unique_ptr<GameScreen>(host));

    eng.changeGameScreen("Title");
    eng.changeGameScreen("Host");
    eng.changeGameScreen("Title");

    Events::triggerEvent("ping", std::make_shared<BasicEvent>());
    // Exactly one increment, not two.
    CHECK_EQ(title->pingHits, 1);
}

TEST_CASE("onExit returning false vetoes the screen change")
{
    resetEvents();
    GameEngine eng;
    auto* a = new StubScreen();
    auto* b = new StubScreen();
    eng.addGameScreen("A", std::unique_ptr<GameScreen>(a));
    eng.addGameScreen("B", std::unique_ptr<GameScreen>(b));

    eng.changeGameScreen("A");
    a->blockExit = true;
    eng.changeGameScreen("B");
    CHECK_EQ(a->exitCallCount, 1);
    CHECK_EQ(b->initCount,     0); // B never ran
    // A's listener still works because A is still active.
    Events::triggerEvent("ping", std::make_shared<BasicEvent>());
    CHECK_EQ(a->pingHits, 1);

    // Allow the exit now -> change goes through.
    a->blockExit = false;
    eng.changeGameScreen("B");
    CHECK_EQ(a->exitCallCount, 2);
    CHECK_EQ(b->initCount,     1);
}

TEST_CASE("change_screen event routes through the bridge listener")
{
    // Mirror of what GameEngine::start() installs: a change_screen
    // listener that calls changeGameScreen. Confirms the screens can
    // navigate without holding a pointer to the engine.
    resetEvents();
    GameEngine eng;
    auto* a = new StubScreen();
    auto* b = new StubScreen();
    eng.addGameScreen("A", std::unique_ptr<GameScreen>(a));
    eng.addGameScreen("B", std::unique_ptr<GameScreen>(b));
    ChangeScreenBridge bridge(eng);

    eng.changeGameScreen("A");
    auto evt = std::make_shared< Event<std::string> >(std::string("B"));
    Events::triggerEvent("change_screen", evt);

    CHECK_EQ(b->initCount, 1);
    // After moving to B, A's ping listener must be gone.
    Events::triggerEvent("ping", std::make_shared<BasicEvent>());
    CHECK_EQ(a->pingHits, 0);
    CHECK_EQ(b->pingHits, 1);
}

TEST_CASE("title -> character -> title cycle matches the production flow")
{
    // Mimics the user-visible navigation Title -> Character -> back to
    // Title (the path "press any button -> select character -> escape
    // back" takes). Verifies init() count and listener cleanliness.
    resetEvents();
    GameEngine eng;
    auto* title     = new StubScreen();
    auto* character = new StubScreen();
    eng.addGameScreen("Title",     std::unique_ptr<GameScreen>(title));
    eng.addGameScreen("Character", std::unique_ptr<GameScreen>(character));
    ChangeScreenBridge bridge(eng);

    auto goTitle = std::make_shared< Event<std::string> >(std::string("Title"));
    auto goChar  = std::make_shared< Event<std::string> >(std::string("Character"));
    Events::triggerEvent("change_screen", goTitle);
    Events::triggerEvent("change_screen", goChar);
    Events::triggerEvent("change_screen", goTitle);

    CHECK_EQ(title->initCount,     2);
    CHECK_EQ(character->initCount, 1);
    // Only the live screen (title) should respond to "ping".
    Events::triggerEvent("ping", std::make_shared<BasicEvent>());
    CHECK_EQ(title->pingHits,     1);
    CHECK_EQ(character->pingHits, 0);
}

// ---------------------------------------------------------------------
// Regression tests for the "extra button press needed when returning to
// title" and "no way out of Local Play" bugs.
//
// These exercise the REAL GametitleScreen / CharacterScreen instead of
// stubs so the behavior we ship matches the behavior we test. The
// screens load fonts + textures via ResourceManager, which falls back
// silently when files are missing -- so the tests don't depend on an
// SFML window or any graphics context.
// ---------------------------------------------------------------------

namespace {

GamepadEvent makeReleased(const std::string& button, int index = 0)
{
    GamepadEvent e;
    e.button = button;
    e.type   = GamepadEvent::RELEASED;
    e.index  = index;
    return e;
}

// Capture every change_screen event so the tests can assert which
// target name was requested.
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

TEST_CASE("GametitleScreen preserves the 'pressed' flag across init() calls")
{
    // This is the direct regression for the "I have to press an extra
    // button when leaving Host or Join" bug. The user passed the
    // press-any-button gate once; returning here from a sub-menu must
    // NOT re-arm the gate.
    resetEvents();
    GametitleScreen title;
    title.setConfig(std::make_shared<Config>());

    // First entry: gate is armed.
    title.init();
    CHECK(!title.hasBeenPressed());

    // User presses a button to acknowledge the title screen.
    auto evt = std::make_shared<GamepadEvent>(makeReleased("A"));
    Events::triggerEvent("gamepad_event", evt);
    CHECK(title.hasBeenPressed());

    // Simulate leaving the title and coming back (Host/Join cancel,
    // Character back-out, etc.) -- the engine re-runs init() every
    // time it enters a screen. The flag must SURVIVE this re-init so
    // the user lands directly on the menu without another press.
    title.init();
    CHECK(title.hasBeenPressed());
}

TEST_CASE("CharacterScreen MENU button fires change_screen('Title')")
{
    // Regression for "I cannot go back if I choose Local Play". MENU
    // (Esc on keyboard, Start on gamepad) must route back to the
    // title; otherwise the player is stuck on the character picker.
    resetEvents();
    CharacterScreen chars;
    auto cfg = std::make_shared<Config>();
    cfg->player_map[0] = 1; // GametitleScreen's confirmMenuSelection
                            // seeds player_map this way for LOCAL PLAY.
    chars.setConfig(cfg);
    chars.init();

    ChangeScreenCapture cap;

    // MENU should immediately route to Title.
    auto evt = std::make_shared<GamepadEvent>(makeReleased("MENU", 0));
    Events::triggerEvent("gamepad_event", evt);

    bool sawTitle = false;
    for (const auto& t : cap.targets) {
        if (t == "Title") { sawTitle = true; break; }
    }
    CHECK(sawTitle);
}

TEST_CASE("CharacterScreen MENU does not register the pressing controller as a new player")
{
    // The MENU handler MUST run before the "new player joining" branch
    // so a wandering player can't be auto-added when they only meant
    // to back out. We use player index 99 because the seeded LOCAL
    // PLAY map only contains index 0.
    resetEvents();
    CharacterScreen chars;
    auto cfg = std::make_shared<Config>();
    cfg->player_map[0] = 1;
    chars.setConfig(cfg);
    chars.init();

    ChangeScreenCapture cap;

    auto evt = std::make_shared<GamepadEvent>(makeReleased("MENU", 99));
    Events::triggerEvent("gamepad_event", evt);

    // Title change fired AND index 99 did NOT get added to the map.
    bool sawTitle = false;
    for (const auto& t : cap.targets) if (t == "Title") sawTitle = true;
    CHECK(sawTitle);
    CHECK_EQ(cfg->player_map.count(99), static_cast<size_t>(0));
}

TEST_CASE("CharacterScreen never lets two players lock in the same character")
{
    // Regression: each couch-co-op player must end up with a unique
    // character. The screen enforces this two ways -- new players land
    // on the first not-selected EMPTY slot, and LEFT/RIGHT cycling
    // skips slots already locked by someone else. Either bug would
    // surface as a duplicate value in config->char_map.
    resetEvents();
    CharacterScreen chars;
    auto cfg = std::make_shared<Config>();
    // Two-player Local Play seeds the first slot the same way
    // GametitleScreen::confirmMenuSelection does.
    cfg->player_map[0] = 1;
    chars.setConfig(cfg);
    chars.init();

    auto fire = [](const std::string& btn, int idx) {
        auto evt = std::make_shared<GamepadEvent>(makeReleased(btn, idx));
        Events::triggerEvent("gamepad_event", evt);
    };

    // P1 (gamepad index 0) is already in the map from init seeding;
    // simulate them locking in their default highlight.
    fire("A", 0);

    // A second controller (index 1) joins -- they get auto-assigned
    // the first unoccupied portrait, which because P1 is hovering
    // slot 0 must be a DIFFERENT slot.
    fire("A", 1); // first press registers them as a new player
    fire("A", 1); // second press locks their highlight

    // The "start game" press fires change_screen('GamePlay') and
    // commits the char_map. Capture it so we can inspect the final
    // assignment without depending on the screen internals.
    ChangeScreenCapture cap;
    fire("A", 0);

    bool sawGamePlay = false;
    for (const auto& t : cap.targets) if (t == "GamePlay") sawGamePlay = true;
    CHECK(sawGamePlay);

    // Two players, two distinct characters, both in {MOM,SIS,BRO,DAD}.
    CHECK_EQ(cfg->num_players, 2);
    CHECK_EQ(cfg->char_map.count(1), static_cast<size_t>(1));
    CHECK_EQ(cfg->char_map.count(2), static_cast<size_t>(1));
    CHECK(cfg->char_map[1] != cfg->char_map[2]);
}

TEST_CASE("CharacterScreen RIGHT cycling skips already-locked slots")
{
    // After P1 locks character at slot 0, P2 RIGHT-cycling from slot 1
    // must NOT be able to wrap back onto slot 0. Drive the screen long
    // enough to observe their final pick is something OTHER than P1's.
    resetEvents();
    CharacterScreen chars;
    auto cfg = std::make_shared<Config>();
    cfg->player_map[0] = 1;
    chars.setConfig(cfg);
    chars.init();

    auto fire = [](const std::string& btn, int idx) {
        auto evt = std::make_shared<GamepadEvent>(makeReleased(btn, idx));
        Events::triggerEvent("gamepad_event", evt);
    };

    // P1 locks.
    fire("A", 0);

    // P2 joins, then cycles RIGHT a few times before locking. The
    // intermediate hovers must never land back on slot 0.
    fire("A", 1); // join
    fire("RIGHT", 1);
    fire("RIGHT", 1);
    fire("RIGHT", 1);
    fire("A", 1); // lock

    ChangeScreenCapture cap;
    fire("A", 0); // start
    bool sawGamePlay = false;
    for (const auto& t : cap.targets) if (t == "GamePlay") sawGamePlay = true;
    CHECK(sawGamePlay);

    CHECK(cfg->char_map[1] != cfg->char_map[2]);
}

TEST_MAIN()
