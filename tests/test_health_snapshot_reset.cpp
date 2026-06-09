// Regression test for the "instant GAME OVER on a new round" bug.
//
// History: a player who reached 0 HP in round N would have that 0 HP
// pushed into NetworkManager every frame by GameplayScreen::
// recordHealthSnapshots(). When round N+1 started,
// GameplayScreen::applyHealthSnapshots() read the snapshot back and
// overwrote the freshly-spawned character's per-class default HP with
// 0 -- triggering the "player_died" event on the very first tick and
// kicking the player straight back to the GameEnd screen. Restart,
// repeat, forever.
//
// What this suite pins down:
//   * shouldApplyHealthSnapshot(snap) returns false for snap <= 0
//     (the policy that fixed the bug at the source).
//   * Mirroring GameplayScreen::applyHealthSnapshots(), a character
//     spawned at maxHealth survives a recorded snapshot of 0 -- i.e.
//     they keep their per-class default HP and the game does not end
//     before it begins.
//   * The mid-game rejoin path still restores real HP values (>0) so
//     the snapshot system isn't broken in the process.
//
// We deliberately do NOT boot a GameplayScreen here: that would pull
// in SFML window/audio context and resource files. Instead we exercise
// the public static contract + mirror the few lines of apply logic,
// the same approach test_spectator.cpp uses for the player_died
// handler.

#include "test_harness.hpp"
#include "engine/NetworkManager.hpp"
#include "game/characters/Character.hpp"
#include "game/screens/GameplayScreen.hpp"

#include <memory>

namespace {

// Build a Character that looks like a freshly init'd player without
// loading sprites/audio. Health = maxHealth, since Character::init()
// would normally set both from ModConfig.
std::shared_ptr<Character> makeFreshlySpawned(int slot, int maxHp)
{
    auto c = std::make_shared<Character>();
    c->setPlayerNumber(slot);
    c->maxHealth = maxHp;
    c->health    = maxHp;
    return c;
}

// Mirror of GameplayScreen::applyHealthSnapshots() for the single-slot
// case. Keeps the test independent of the screen's private bookkeeping
// while still exercising the public shouldApplyHealthSnapshot()
// contract that both call sites in GameplayScreen.cpp share.
void applySnapshotIfUsable(Character& c)
{
    const int snap =
        NetworkManager::instance().slotHealthSnapshot(c.player_number);
    if (!GameplayScreen::shouldApplyHealthSnapshot(snap)) return;
    c.health = snap;
    if (c.health > c.maxHealth) c.health = c.maxHealth;
}

void resetSnapshots()
{
    NetworkManager::instance().clearHealthSnapshots();
}

} // namespace

TEST_CASE("shouldApplyHealthSnapshot rejects sentinel and dead values")
{
    // -1 is the "no snapshot recorded" sentinel from
    // NetworkManager::slotHealthSnapshot(). 0 is the bug we are
    // fixing -- a player who died in the previous round. Both must
    // be treated as "do not restore".
    CHECK(!GameplayScreen::shouldApplyHealthSnapshot(-1));
    CHECK(!GameplayScreen::shouldApplyHealthSnapshot(0));
}

TEST_CASE("shouldApplyHealthSnapshot accepts any positive HP")
{
    CHECK(GameplayScreen::shouldApplyHealthSnapshot(1));
    CHECK(GameplayScreen::shouldApplyHealthSnapshot(3));
    CHECK(GameplayScreen::shouldApplyHealthSnapshot(99));
}

TEST_CASE("New round does not inherit a 0-HP snapshot from the last round")
{
    // Simulate the end of the previous round: the player hit 0 HP and
    // recordHealthSnapshots() stored that value before the GameEnd
    // screen change.
    resetSnapshots();
    NetworkManager::instance().recordSlotHealth(/*slot=*/1, /*health=*/0);

    // New round starts: a fresh Character is spawned at full HP and the
    // engine runs applyHealthSnapshots() over it.
    auto c = makeFreshlySpawned(/*slot=*/1, /*maxHp=*/3);
    applySnapshotIfUsable(*c);

    // Without the fix this would be 0 -- player_died fires immediately,
    // the round ends on tick 1, and the loop repeats next restart.
    CHECK_EQ(c->health, 3);
    resetSnapshots();
}

TEST_CASE("Mid-game rejoin still restores a live snapshot")
{
    // The snapshot system exists for network drop+rejoin: a player who
    // disconnects at 2/3 HP should spawn back at 2/3 HP, NOT at full.
    // This test pins down that the fix above didn't disable that path.
    resetSnapshots();
    NetworkManager::instance().recordSlotHealth(/*slot=*/2, /*health=*/2);

    auto c = makeFreshlySpawned(/*slot=*/2, /*maxHp=*/3);
    applySnapshotIfUsable(*c);

    CHECK_EQ(c->health, 2);
    resetSnapshots();
}

TEST_CASE("Snapshot above maxHealth is clamped down")
{
    // Defensive: ModConfig hot-reload could shrink a character's
    // maxHealth between rounds. The apply path clamps so we never spawn
    // someone with more HP than their class allows.
    resetSnapshots();
    NetworkManager::instance().recordSlotHealth(/*slot=*/3, /*health=*/99);

    auto c = makeFreshlySpawned(/*slot=*/3, /*maxHp=*/3);
    applySnapshotIfUsable(*c);

    CHECK_EQ(c->health, 3);
    resetSnapshots();
}

TEST_MAIN();
