# src/engine/

Implementations for the engine internals declared in
[include/engine/](../../include/engine/). See that folder's
[README](../../include/engine/README.md) for the responsibility of each
class — this README documents only the impl-side notes.

## Contents

| File | Header | Notes |
|---|---|---|
| `ClueReader.cpp` | [ClueReader.hpp](../../include/engine/ClueReader.hpp) | Holds the parsed XML buffer in `xmlContent` so RapidXML's non-owning `char*` references stay valid. Reads from `Paths::resource("items.xml")`. |
| `Env.cpp` | [Env.hpp](../../include/engine/Env.hpp) | Single-file shim around `_dupenv_s` (MSVC) / `getenv` (POSIX). Honors a couple of truthy strings (`1`, `true`, `yes`, `on`) in `Env::asBool`. |
| `EventManager.cpp` | [EventManager.hpp](../../include/engine/EventManager.hpp) | Pub/sub with sync (`triggerEvent`) and async (`queueEvent` + `notify`) delivery. `Events::clear()` is the per-test reset point. Reentrant-safe: subscriber lists snapshot before dispatch. |
| `GameEngine.cpp` | [GameEngine.hpp](../../include/engine/GameEngine.hpp) | The main loop. Owns the `sf::RenderWindow`, the screen stack, and the fixed-step accumulator. Handles fullscreen toggles, target FPS, and screen change dispatch via the `change_screen` event. |
| `GameObject.cpp` | [GameObject.hpp](../../include/engine/GameObject.hpp) | Tiny base class — most logic is inline in the header. |
| `Gamepad.cpp` | [Gamepad.hpp](../../include/engine/Gamepad.hpp) | Polls SFML joystick + keyboard each frame, debounces, and emits `GamepadEvent`s. Handles the player-number assignment (slot 0 = keyboard / first joystick to press anything; subsequent joysticks fill 1-3). |
| `GameScreen.cpp` | [GameScreen.hpp](../../include/engine/GameScreen.hpp) | Base-class plumbing for screens — `subscribe()`, `clearSubscriptions()`, and the default no-op `init` / `onEnter` / `onExit`. |
| `ModConfig.cpp` | [ModConfig.hpp](../../include/engine/ModConfig.hpp) | RapidXML parser + defaults. `applyDefaults()` reproduces vanilla bit-for-bit so a missing / malformed `mods.xml` leaves the game unchanged. |
| `NetworkManager.cpp` | [NetworkManager.hpp](../../include/engine/NetworkManager.hpp) | TCP host/join handshake, RNG seed broadcast, per-tick input edge buffering. In `OFFLINE` mode every method short-circuits, so the local code path is undisturbed. |
| `Random.cpp` | [Random.hpp](../../include/engine/Random.hpp) | Park-Miller `Equilikely` / `SelectStream` / `PlantSeeds` lifted verbatim from the original college project. |
| `ResourceManager.cpp` | [ResourceManager.hpp](../../include/engine/ResourceManager.hpp) | `std::map<std::string, std::shared_ptr<...>>` caches for fonts / textures / sound buffers. First load reads from disk; subsequent loads return the cached pointer. `clear()` is the per-test reset point. |

Headers without an accompanying `.cpp`: `Constants.hpp`, `Engine.hpp`,
`EngineEvents.hpp`, `Paths.hpp`, `RandomUtil.hpp` — all header-only.

## Conventions

* No `#include` of anything under `include/game/` from this folder.
* All file paths come through `Paths::resource(rel)` so the relocation
  is one-stop.
* All env vars come through `Env::` so MSVC's `_dupenv_s` mess is
  hidden.
