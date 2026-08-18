#ifndef VILLAIN_HPP
#define VILLAIN_HPP

#include <memory>
#include <string>
#include <vector>
#include <SFML/Graphics.hpp>
#include "engine/Engine.hpp"
#include "components/SpriteAnimation.hpp"
#include "components/Hitbox.hpp"
#include "game/rooms/Room.hpp"
#include "game/rooms/RoomGroup.hpp"
#include "game/Config.hpp"
#include "game/characters/Character.hpp"
#include "game/characters/VillainDirector.hpp"
////////////////
// Villain.hpp
//
// Alien: Isolation-style two-tier ghost AI.
//
//   * The Villain (this class, the "hunter") deliberately KNOWS NOTHING
//     about player positions. Once per `pingInterval` seconds it pings
//     the Director and caches the answer as a wander hint.
//   * The Director (`VillainDirector`, owned here) has perfect
//     information and answers "who is closest to this point?" on
//     demand.
//
// Behavior summary per frame:
//
//   1. Tick the ping timer. If it elapsed, call the Director, cache the
//      hint position, and reset the timer.
//   2. If a character is in the same room as the ghost AND is eligible
//      (alive, not invul, not stationary-SIS), lock onto them and chase
//      until they leave the room (or die / become invul). The lock
//      survives ping cycles -- the ghost does NOT re-evaluate "is this
//      really the closest target?" every tick.
//   3. Otherwise, wander -- but bias the next room choice toward the
//      cached Director hint, so the ghost still gravitates toward
//      players over time.
//
// Difficulty controls how often the ghost pings the Director. Easy =
// rare pings (stale info, easy to lose), Hard = frequent pings (the
// ghost almost always knows where you are).
//
// Base wander and chase speeds are below the slowest player. After 90
// and 180 seconds of haunting, pressure levels shorten Director pings
// and increase speed so an endless kite eventually becomes unsafe.
//
////////////////

class Villain : public Character
{
public:
    void init() override;
    void onUpdate(float dt) override;
    void onDraw(sf::RenderTarget& target, sf::RenderStates states) const override;
    void wander();
    void chase();
    void returnToCenter();
    void setDirection();
    void hurt() override;
    void setItemDamage(int damage) override;
    bool checkCharacters();
    bool isVillain() override { return true; }

    // Picks the ping interval and any other difficulty-scaled
    // parameters. Safe to call before or after init(); init() will
    // honor whichever interval is current when it runs.
    void setDifficulty(Config::DIFFICULTY d);

    // Shadows Character::setEntities so the Villain's owned Director
    // ALWAYS sees the same entity group the Villain itself does.
    // Without this, callers that go through setEntities without then
    // running init() (such as unit tests) would leave the Director's
    // entity pointer null and silently get nullptr back from every
    // nearestTargetCharacter() call.
    void setEntities(EntityGroup* entities) {
        Character::setEntities(entities);
        director_.setEntities(entities);
    }

    // Base movement speeds in pixels per second. Pressure multipliers
    // are applied separately as the haunt level rises.
    static double wanderSpeed();
    static double chaseSpeed();

    // Ping intervals in seconds per difficulty. Lower = ghost re-asks
    // the Director (and therefore re-targets) more often.
    static float pingIntervalFor(Config::DIFFICULTY d);
    static int hauntLevelFor(float elapsedSeconds);
    static double pressureMultiplierFor(int hauntLevel);
    static float wrongWeaponPressurePenalty(int damage);

    // Find the closest living, targetable character anywhere on the
    // map. Thin wrapper around the owned Director; retained because
    // tests and any future game code may still want a single-shot
    // "who is closest right now?" query. The AI itself reaches the
    // Director through the ping cadence, NOT this method.
    Character* nearestTargetCharacter() const;

    // ---- Test hooks. None of these affect game behavior. ----

    // Owned Director (the all-knowing AI). Tests poke this directly.
    VillainDirector& director() { return director_; }
    const VillainDirector& director() const { return director_; }

    // Force a Director ping right now and apply the result to the
    // wander hint. Production code uses the pingTimer; tests use this
    // to skip the timer math.
    void pingDirector();

    // Seconds between pings (driven by setDifficulty).
    float pingInterval() const { return pingIntervalSec_; }
    int hauntLevel() const { return hauntLevel_; }
    float hauntElapsed() const { return hauntElapsedSec_; }
    // Seconds elapsed since the last ping. Test hook.
    float timeSinceLastPing() const { return pingTimerSec_; }
    // Does the ghost currently have a wander hint cached from the
    // Director?
    bool  hasDirectorHint() const { return hasDirectorHint_; }
    // The last hint position the Director handed out. Undefined when
    // hasDirectorHint() is false.
    sf::Vector2f directorHint() const { return directorHintPos_; }
    // The character the ghost is currently locked onto in chase mode,
    // or nullptr when wandering.
    Character* lockedTarget() const { return lockedCharacter_; }
    // Allow tests to drive the ping cadence by advancing the internal
    // timer without running the full onUpdate() pipeline.
    void advancePingTimer(float dt) { pingTimerSec_ += dt; }
    void advanceHauntTimer(float dt);
    // Test hook: run only the room-scan / target-lock half of the
    // per-frame work, returning true when a target is locked.
    bool runRoomScanForTest() { return checkCharacters(); }

protected:
    int healthCut = 0;
    int randint = 0;
    // Intentional shadow of Character::health: Villain's lifecycle is independent
    // of the base Character health (Character::init() is not called for villains).
    int health = 0;
    std::string previousString;
    std::vector<std::string> possiblerooms;
    bool isChasing = false;
    bool started = false;
    bool needsCentering = false;
    bool fastSpeed = false;
    sf::FloatRect roomHbox;
    sf::FloatRect chaseHbox;

    // When no target is in the current room, checkCharacters() falls
    // back to the cached Director hint and stashes it here so
    // setDirection() can bias the next room choice toward the closest
    // known player. Cleared (hasChaseHint = false) when the Director
    // also has no candidate, so the wandering behavior remains
    // identical when the entire team is dead.
    bool hasChaseHint = false;
    sf::Vector2f chaseHintTarget;

    // ---- Two-tier AI state. ----
    VillainDirector director_;
    float pingIntervalSec_ = 4.0f;   // NORMAL default; setDifficulty overwrites.
    float basePingIntervalSec_ = 4.0f;
    float pingTimerSec_    = 0.f;    // Counts up; ping fires when >= pingIntervalSec_.
    float hauntElapsedSec_ = 0.f;
    int hauntLevel_ = 0;
    sf::Vector2f directorHintPos_;
    bool hasDirectorHint_  = false;
    // Non-owning pointer into entity_group's characters. Cleared when
    // the locked character leaves the room, dies, becomes invul, or
    // (for SIS) stops moving.
    Character* lockedCharacter_ = nullptr;

    // Shadows Character::death_map. Kept as a pointer into the central
    // texture cache for the same reason described in Character.hpp.
    sf::Texture* death_map = nullptr;
    SpriteAnimation death_animation;
    hh::Sound pressure_sound;

private:
    // Speed setters that keep `speed` and `fastSpeed` in sync. The
    // original code scattered raw `speed *= 1.25` and `speed /= 1.5`
    // calls across half a dozen sites, which drifted out of sync;
    // these two helpers make the transitions explicit.
    void enterWanderMode();
    void enterChaseMode();
};

#endif
