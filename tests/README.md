# tests/

CTest-driven regression suite. The structure here is dead-simple:

* Every `test_*.cpp` becomes its own executable (see [CMakeLists.txt](CMakeLists.txt)
  for the `file(GLOB ...)` loop).
* Each executable links against `HouseHaunters_core` (the same static
  library the game uses) and `hh_compile_options` (so the same `/W4`
  warnings the game is built with apply to test code as well).
* Each executable runs from `${CMAKE_BINARY_DIR}` so paths like
  `../resources/...` resolve to the project's [resources/](../resources/)
  folder, exactly the way they do for the real `HH.exe`.

CTest is wired into the default build via a custom `hh_run_tests ALL`
target, so a plain `cmake --build .` (or `_build_tests.bat`) re-runs every
test. Set `-DHH_RUN_TESTS_ON_BUILD=OFF` to build-only.

## Test map

| File | What it covers |
|---|---|
| `test_character_lobby.cpp` | Local-play lobby gate (wait for P1) + hot-plug auto-add of P2/P3/P4 |
| `test_clue_reader.cpp` | RapidXML parsing of `resources/items.xml`; deterministic clue selection |
| `test_config.cpp` | `Config::CHARACTER` and `Config::DIFFICULTY` enum / map invariants |
| `test_entity_group.cpp` | Character add/remove, slot lookup, villain detection |
| `test_events.cpp` | Sync/queued event delivery, subscriber removal, RAII subscription |
| `test_gameobject.cpp` | Base `GameObject` lifecycle |
| `test_gamepad_hotplug.cpp` | `GamepadController` slot bookkeeping, disable/enable, no spurious hot-plug events |
| `test_gamepad_layouts.cpp` | Controller VID/PID detection + per-layout button-map population |
| `test_hitbox.cpp` | Rectangle overlap math used for swing collisions |
| `test_join_code.cpp` | Networked-lobby join code encoder/decoder |
| `test_mod_config.cpp` | `ModConfig` defaults, file load, partial overrides (characters / villain / lobby / audio), malformed-input fallbacks |
| `test_network_host_join.cpp` | Real `sf::TcpListener`/`TcpSocket` end-to-end host/join flow (uses a free port) |
| `test_network_manager.cpp` | `NetworkManager` env-var parsing, tick edge buffering, slot remapping |
| `test_paths.cpp` | `Paths::resource()` resolution |
| `test_random.cpp` | `PlantSeeds` + `randomInt` distribution invariants |
| `test_room_generation.cpp` | Pure mansion-zone classification and themed room pools |
| `test_screen_flow.cpp` | `GameEngine::changeGameScreen`, subscription cleanup, re-entry init |
| `test_spectator.cpp` | `PlayerView` target follow + spectator cycling + GAME OVER contract |
| `test_villain_ai.cpp` | Alien: Isolation-style two-tier AI: target eligibility, difficulty-scaled ping cadence, cached hints, base speeds, timed HAUNT escalation, and every character's endgame passive. |
| `test_villain_chase.cpp` | `Villain::nearestTargetCharacter` (now a thin wrapper around the Director) picks closest live player; skips dead/invul/stationary-SIS |
| `test_build_atlas.py` | Optional Python test for [tools/build_atlas.py](../tools/build_atlas.py) (skipped if no `python3`) |

## Adding a new test

```cpp
// tests/test_my_thing.cpp
#include "test_harness.hpp"
#include "engine/Foo.hpp"

TEST_CASE("my thing: it works") {
    CHECK_EQ(2 + 2, 4);
}

TEST_MAIN()
```

That's it — re-run CMake (CONFIGURE_DEPENDS picks it up) and the new file
becomes its own CTest case automatically.

The tiny in-tree framework lives in [test_harness.hpp](test_harness.hpp);
provided macros are `TEST_CASE`, `CHECK`, `CHECK_EQ`, `REQUIRE`, and
`TEST_MAIN()`. No third-party deps.

## Conventions

* Tests must not depend on an SFML window or graphics context.
  `ResourceManager` falls back silently when files are missing, but
  anything that requires a live OpenGL context (e.g. constructing an
  `sf::RenderWindow`) will not work in CI.
* Reset global event state with `Events::clear()` between tests in the
  same executable if you subscribe.
* For tests that touch disk, use paths relative to the build root
  (`"../resources/..."` matches both the game launch convention and the
  CTest `WORKING_DIRECTORY` setting in [CMakeLists.txt](CMakeLists.txt)).
