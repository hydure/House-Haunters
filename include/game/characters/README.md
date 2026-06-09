# include/game/characters/

The playable characters, the antagonist they're running from, and the
per-player camera/HUD view.

## Contents

| Header | Purpose |
|---|---|
| `Character.hpp` | Base class for the four playable family members. Encodes the shared movement, attack, clue-pickup, and damage logic. Per-character differences (HP, speed, spritesheet position) are pulled from `ModConfig::character(...)` in `Character::init()` so data-driven mods can tune stats without touching code. Behavioral specials (Brother sprint, Sister stealth, Father jackpot damage, Mother clue upgrade) stay keyed off `Character::character` in the relevant methods. |
| `Villain.hpp` | The ghost ("hunter" half of the Alien: Isolation-style AI). `Character` subclass that overrides `init`, `onUpdate`, `onDraw`, `hurt`, `setItemDamage`, and `isVillain()` to return true. Owns a `VillainDirector` it pings on a difficulty-scaled cadence (EASY 8.0s / NORMAL 4.0s / HARD 1.5s) for a wander hint. Latches onto same-room characters and keeps chasing them until they leave. Wander and chase speeds (`Villain::wanderSpeed()` / `chaseSpeed()`) are both strictly slower than the slowest player so kiting is always possible. Sprite path, frame size, walk-frame indices, and starting HP come from `ModConfig::villain()`. |
| `VillainDirector.hpp` | The "all-knowing" half of the ghost AI. Lightweight stateless query object: holds a non-owning pointer to the `EntityGroup` and answers `nearestTo(origin)` with the closest eligible character (skipping the villain, dead/invul characters, and the Sister while she's standing still). The Villain consults it on a ping cadence; nothing in the Director ever moves anyone. |
| `PlayerView.hpp` | Per-player camera + HUD. One instance per active player. Follows the player's character every frame, draws the heart bar / clue overlay, and routes that gamepad's input to the right character. When the player dies, switches into spectator mode (LEFT/RIGHT cycles surviving teammates); see `currentTarget()`, `spectatorTarget()`, `cycleSpectator()`, `ensureSpectator()`. |

## Key design notes

* **No per-character subclasses.** All four characters use the same
  `Character` class; the `Config::CHARACTER` enum value (BRO / SIS / DAD /
  MOM) is what picks the sprite slot, HP, speed, and ability path. Adding
  a fifth ability would mean either branching in `Character::init` /
  `onGamepadEvent` or introducing a real strategy object.
* **Mod-driven sprite/stat data.** `Character::init` and `Villain::init`
  both consult [ModConfig](../../engine/ModConfig.hpp). The defaults in
  `ModConfig::applyDefaults()` reproduce the historic hardcoded values
  bit-for-bit, so an unmodded run is unchanged.
* **Spectator mode.** Built into `PlayerView`, not `Character` — a dead
  player still has a slot in `EntityGroup`, but their `PlayerView` is
  what hops the camera onto a surviving teammate. The `player_died`
  event still fires from `Character::onUpdate` the moment HP hits zero,
  so the GAME OVER screen logic in `GameplayScreen` is unaffected.

## Adding a new character

For art-only changes (re-skin, stat tweak), edit [resources/mods.xml](../../../resources/mods.xml).

For a truly new ability:

1. Add a `Config::CHARACTER` enum value in [include/game/Config.hpp](../Config.hpp).
2. Add a `case` to the relevant method in [src/game/characters/Character.cpp](../../../src/game/characters/Character.cpp)
   (most abilities are gated in `onGamepadEvent`, `checkClues`, or
   `Villain::checkCharacters`).
3. Add a `<character id="...">` block to the default `resources/mods.xml`
   so the lobby has a portrait to show.
4. Cover it with a test in [tests/](../../../tests/).
