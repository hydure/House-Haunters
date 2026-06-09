// Gamepad hot-plug tests.
//
// What this exercises:
//   * addGamepads() always reserves a keyboard slot, even when no
//     physical pads are connected (the common case on the build agent).
//   * removeGamepad / disable / enable round-trip flips the right flags
//     and updates count.
//   * reconcileConnections() does NOT emit a spurious connect/disconnect
//     event when nothing has changed between two calls. (We can't fake
//     a physical hot-plug from a unit test, but we can guard against
//     the "every frame, every controller appears to disconnect" bug.)
//
// All tests are state-only; nothing here pokes the SFML window or audio.

#include "test_harness.hpp"
#include "engine/Gamepad.hpp"
#include "engine/EventManager.hpp"

namespace {

struct EventCounter {
    int connects    = 0;
    int disconnects = 0;
    long connectSub    = 0;
    long disconnectSub = 0;

    EventCounter()
    {
        connectSub = Events::addEventListener("gamepad_connect",
            [this](base_event_type) { ++connects; });
        disconnectSub = Events::addEventListener("gamepad_disconnect",
            [this](base_event_type) { ++disconnects; });
    }
    ~EventCounter()
    {
        Events::removeEventListener(connectSub);
        Events::removeEventListener(disconnectSub);
    }
};

void resetEvents()
{
    Events::clear();
}

} // namespace

TEST_CASE("addGamepads reserves a keyboard slot")
{
    resetEvents();
    GamepadController gc;
    gc.addGamepads();
    // At minimum the keyboard slot must exist after addGamepads().
    // count tracks physical pads only -- it can be 0 on CI -- so we
    // assert on slotCount() (which includes the keyboard slot).
    CHECK(gc.slotCount() >= 1);
    // Keyboard is parked at index == (highest physical index + 1) so
    // on a no-pad agent that's index 0.
    CHECK(gc.has(gc.count));
}

TEST_CASE("removeGamepad on the keyboard slot drops the slot")
{
    resetEvents();
    GamepadController gc;
    gc.addGamepads();
    const int kbIndex = gc.count;
    REQUIRE(gc.has(kbIndex));
    gc.removeGamepad(kbIndex);
    CHECK(!gc.has(kbIndex));
}

TEST_CASE("removeGamepad on an unknown index is a no-op")
{
    resetEvents();
    GamepadController gc;
    gc.addGamepads();
    const int slotsBefore = gc.slotCount();
    gc.removeGamepad(12345); // not registered
    CHECK_EQ(gc.slotCount(), slotsBefore);
}

TEST_CASE("disable / enable round-trip toggles isActive without removing slots")
{
    resetEvents();
    GamepadController gc;
    gc.addGamepads();
    const int kbIndex = gc.count;
    Gamepad* kb = gc.getGamepad(kbIndex);
    REQUIRE(kb != nullptr);
    CHECK(kb->isActive());
    gc.disableGamepads({kbIndex});
    CHECK(!kb->isActive());
    gc.enableGamepads({kbIndex});
    CHECK(kb->isActive());
    // Slot count never changed; only the active flag was flipped.
    CHECK(gc.has(kbIndex));
}

TEST_CASE("getGamepad returns nullptr for an unknown index")
{
    resetEvents();
    GamepadController gc;
    gc.addGamepads();
    CHECK(gc.getGamepad(99) == nullptr);
}

TEST_CASE("reconcileConnections does not emit spurious events when nothing changed")
{
    // The bug we're guarding against: a frame-by-frame reconcile that
    // mistakes "no change" for "every pad just connected / disconnected".
    // We can't fake a real SFML hot-plug here, but we CAN call reconcile
    // twice and assert the second call queues zero new events.
    resetEvents();
    GamepadController gc;
    gc.addGamepads();
    EventCounter ec;
    // First reconcile may catch up to whatever SFML reports; clear any
    // events it queued so we measure only the second call.
    gc.reconcileConnections();
    Events::notify();
    ec.connects    = 0;
    ec.disconnects = 0;
    gc.reconcileConnections();
    Events::notify();
    CHECK_EQ(ec.connects, 0);
    CHECK_EQ(ec.disconnects, 0);
}

TEST_MAIN()
