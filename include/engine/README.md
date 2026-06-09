# include/engine/

Engine internals: anything that's about *running* a game, not about *this*
game. Screen lifecycle, event bus, resource cache, input, networking, RNG,
and the mod loader all live here. None of these classes know about
`Character`, `Villain`, or any specific screen.

## Contents

| Header | Purpose |
|---|---|
| `ClueReader.hpp` | RapidXML-backed loader for `resources/items.xml`. Picks a high- and low-damage item per game and surfaces the clue strings by tier. |
| `Constants.hpp` | Engine-wide magic numbers (tile size, room dimensions, fade rate, default window size) in one place so art changes touch one file. |
| `Engine.hpp` | Umbrella header that pulls in `GameEngine`, `GameScreen`, `GameObject`, `EventManager`, `ResourceManager`, `Gamepad`, and friends — handy for screens that need most of the engine. |
| `EngineEvents.hpp` | The engine-defined event type vocabulary (`BasicEvent`, `Event<T>`, `GamepadEvent`). |
| `Env.hpp` | Tiny portable env-var reader (`Env::get`, `Env::tryGet`, `Env::asBool`). Hides the MSVC `_dupenv_s` / POSIX `getenv` split. |
| `EventManager.hpp` | The pub/sub bus (`Events::addEventListener`, `removeEventListener`, `triggerEvent` (sync), `queueEvent` + `notify` (async)). Reentrancy-safe; RAII `EventSubscription` wraps an id and unregisters on destruction. |
| `GameEngine.hpp` | The engine itself — owns the SFML window, the screen stack, the gamepad state, and the main loop. `addGameScreen`, `changeGameScreen`, `setTargetFps`, `setFullscreen`. |
| `GameObject.hpp` | Base class for anything drawable / updatable. Provides `init()`, `onUpdate(dt)`, `onDraw(ctx, states)`, and a `z_index` for draw ordering. |
| `Gamepad.hpp` | Joystick + keyboard polling. Translates raw SFML input into the abstract `GamepadEvent` (`PRESSED`/`RELEASED` × button name × player slot) that screens subscribe to. Layout is auto-detected by USB VID/PID for PlayStation (DualShock 3/4, DualSense), Xbox (360, One, Series), Steam Controller, Switch Pro, 8BitDo, and common third-party Xbox-style pads, with a generic SDL fallback. `GamepadController::reconcileConnections()` runs every frame and emits `gamepad_connect` / `gamepad_disconnect` events so screens (especially the lobby) can react to hot-plug. |
| `GameScreen.hpp` | Base class for every screen. Owns a `Config` pointer, a list of `EventSubscription`s released on screen exit, and the `init` / `onUpdate` / `onDraw` lifecycle. |
| `ModConfig.hpp` | Singleton XML-backed mod loader. Reads `resources/mods.xml` once at startup; exposes character stats / sprite paths, villain sprite info, lobby layout, and audio cue paths. Falls back to vanilla defaults when the file is missing or malformed. See [MODDING.md](../../MODDING.md). |
| `NetworkManager.hpp` | Opt-in TCP lockstep networking. Parses `HH_NET`, performs a host/client handshake, broadcasts the RNG seed, and routes per-tick input edges. In `OFFLINE` mode every call is a no-op so the local code path is unaffected. |
| `Paths.hpp` | One-liner that prepends `"../resources/"` to a relative asset path. Centralizes the convention so a future relocation only touches this file. |
| `Random.hpp` | The legacy `Equilikely` / `SelectStream` / `PlantSeeds` API from Park-Miller. |
| `RandomUtil.hpp` | Friendly wrappers (`randomInt(n)`, etc.) over the `Random` API. |
| `ResourceManager.hpp` | Process-wide caches for fonts, textures, and sound buffers keyed by full path. First load reads from disk; subsequent loads are O(1). |

## Conventions

* Engine classes are typically singletons accessed through a static
  `instance()` (or, for the older style, free functions in a namespace).
  This is deliberate: there's only ever one event bus and one resource
  cache per process. Tests reset relevant globals between cases
  (`Events::clear()`, `ResourceManager::clear()`, `ModConfig::reset()`).
* No engine class includes anything from `include/game/`. The arrow goes
  the other way.
* `GameScreen::subscribe(...)` returns an RAII handle that is released
  when the screen changes; this is the preferred way to listen for
  events from a screen.

## Adding a new engine subsystem

1. Header here, implementation in [src/engine/](../../src/engine/).
2. Add it to `Engine.hpp` if it's something every screen will use.
3. Write a test in [tests/](../../tests/) that constructs the subsystem
   without an SFML window (use `ResourceManager`'s missing-file fallback
   if you need to touch assets).
