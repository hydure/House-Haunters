# include/game/objects/

Interactive world objects players can touch, pick up, or read. These are
distinct from `Character` (the player avatars) and `Room` (the floor
plan) — they're free-standing entities that live inside rooms and emit
events when interacted with.

## Contents

| Header | Purpose |
|---|---|
| `Clue.hpp` | A readable clue scattered in a room. Owns its tier (`worthless` / `vague` / `specific` / `jackpot`) and the human-readable string `ClueReader` filled in. When a `Character` overlaps a clue and presses the interact button, the clue raises a `read_clue` event with itself as the payload. |

## Lifecycle

Clues are owned by [EntityGroup](../../components/EntityGroup.hpp) — the
gameplay screen creates them up-front, hands references to characters via
`Character::currentClue`, and `EntityGroup` deletes them at the end of the
run. Don't `delete` a `Clue` from outside `EntityGroup`.

## Adding a new world object

1. Header here, implementation in [src/game/objects/](../../../src/game/objects/).
2. Subclass `GameObject` (from [include/engine/GameObject.hpp](../../engine/GameObject.hpp))
   so it participates in the engine's draw/update loop.
3. If it's interactable, raise an event (e.g. `Events::triggerEvent("door_opened", ...)`)
   instead of calling into the gameplay screen directly. Screens
   subscribe via `GameScreen::subscribe(...)` which is RAII-cleaned on
   screen exit.
4. Either let `EntityGroup` own it (preferred — that's how `Clue`
   works), or store it on the screen that creates it.
