# src/game/characters/

Implementations for the characters, the villain, and the per-player
view. See [include/game/characters/README.md](../../../include/game/characters/README.md)
for class responsibilities.

## Contents

| File | Header | Notes |
|---|---|---|
| `Character.cpp` | [Character.hpp](../../../include/game/characters/Character.hpp) | `init()` pulls per-character stats and spritesheet info from `ModConfig::instance().character(...)`. Watch the local variable name `mod` — it's the int frame-row offset (`3*x + 24*y`) consumed by every `walk_*.addFrames({{mod+1},...})` call. The mod-config reference is named `cmod` to avoid shadowing. |
| `Villain.cpp` | [Villain.hpp](../../../include/game/characters/Villain.hpp) | `init()` is mod-driven via `ModConfig::instance().villain()`. Uses the local helper `toFrameLists(const std::vector<int>&)` to reshape the flat frame list ModConfig hands back into the `vector<vector<int>>` shape `addFrames()` expects. |
| `PlayerView.cpp` | [PlayerView.hpp](../../../include/game/characters/PlayerView.hpp) | Camera follow + HUD draw + spectator routing. Subscribes to gamepad LEFT / RIGHT for spectator-target cycling and to `player_died` to switch into spectator mode the frame the player's character dies. |

## Test coverage

Spectator behavior and character init are covered by
[tests/test_spectator.cpp](../../../tests/test_spectator.cpp) and
[tests/test_entity_group.cpp](../../../tests/test_entity_group.cpp). The
ModConfig contract that backs the data-driven init is exhaustively
exercised in [tests/test_mod_config.cpp](../../../tests/test_mod_config.cpp).
