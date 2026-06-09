# src/game/objects/

Implementations for the interactive world objects declared in
[include/game/objects/](../../../include/game/objects/).

## Contents

| File | Header | Notes |
|---|---|---|
| `Clue.cpp` | [Clue.hpp](../../../include/game/objects/Clue.hpp) | A pickup-able / readable clue. On interaction it raises a `read_clue` event carrying itself as the payload; the active `PlayerView` listens for that event and renders the corresponding clue string. |

## Conventions

* World objects are owned by [EntityGroup](../../../include/components/EntityGroup.hpp).
  Don't `delete` them out from under their owner.
* If an object reacts to player input or proximity, emit an event
  (via `Events::triggerEvent(...)`) rather than calling into the active
  screen directly — screens subscribe via `GameScreen::subscribe(...)`
  which auto-releases on screen exit.
