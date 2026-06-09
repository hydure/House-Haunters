# include/game/

House-Haunters-specific code. Anything in this folder knows about
characters, rooms, clues, or the lobby flow. Promote a class up into
[components/](../components/) or [engine/](../engine/) once it stops
having game-specific dependencies.

## Subdirectory map

| Folder | What's in it |
|---|---|
| [characters/](characters/README.md) | `Character`, `Villain`, `PlayerView` (per-player camera/HUD). |
| [objects/](objects/README.md) | Interactive world objects — currently just `Clue`. |
| [rooms/](rooms/README.md) | `Room` (one tile of the procedural mansion) and `RoomGroup` (the collection of them). |
| [screens/](screens/README.md) | Every `GameScreen` subclass: story, title, character select, gameplay, end, controls, host lobby, join lobby, pause menu. |

## Top-level headers

| File | Purpose |
|---|---|
| `Config.hpp` | Shared per-run configuration: target FPS, window size, fullscreen flag, player↔gamepad map, player↔character map, difficulty. Held by `HouseHauntersGame` and passed to every screen via `GameScreen::setConfig`. |
| `JoinCode.hpp` | Five-character room code used by the networked lobby. Encoder + decoder pair, exhaustively round-tripped in [tests/test_join_code.cpp](../../tests/test_join_code.cpp). |

## Dependency rules

The intended dependency direction is `screens → characters/objects/rooms
→ engine → components`. Screens own gameplay objects, gameplay objects
talk to engine subsystems, and the lowest layer is the generic engine
itself. Don't introduce a back-edge (e.g. an engine class that includes
a screen header) — split out a shared interface instead.

## Adding a new gameplay system

1. If it's a `GameScreen`: see [screens/README.md](screens/README.md).
2. If it's an interactive world object: header in [objects/](objects/),
   impl in [src/game/objects/](../../src/game/objects/).
3. If it's a new playable character variant: today this is data-driven
   via [resources/mods.xml](../../resources/mods.xml) (no subclassing
   needed). See [MODDING.md](../../MODDING.md). For a class-based
   variant with truly new behavior, subclass `Character` in
   [characters/](characters/).
