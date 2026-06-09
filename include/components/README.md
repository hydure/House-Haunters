# include/components/

Generic reusable building blocks that any game could use. Nothing here
references `Character`, `Villain`, the lobby, or any other House
Haunters specifics — promote a class out of `game/` and into here only
when it has no game-specific dependencies left.

## Contents

| Header | Purpose |
|---|---|
| `EntityGroup.hpp` | Owning collection of `Character` pointers. Provides slot-based lookup (`getCharacter(playerNumber)`), bulk iteration, and helpers for distinguishing players from villains. Used by every gameplay system that needs to ask "which characters exist right now?". |
| `Hitbox.hpp` | Axis-aligned rectangle that tracks a parent `GameObject`'s position. Drives attack-swing overlap detection and clue pickup proximity. `Hitbox::follow(parent)` keeps it pinned to its owner across frames. |
| `SpriteAnimation.hpp` | Frame-sequenced sprite playback. Owns a reference to a `sf::Texture` spritesheet, accepts a list of frames as `{{idx}, {idx}, ...}`, and steps through them in `nextFrame(dt)`. Supports one-shot animations with a completion callback. |

## When to add here vs. `game/`

Use this folder when:

* The class would be useful in a different game with the same engine.
* It has no `Character`, `Villain`, `Room`, or screen-specific dependencies.

Use `game/` instead when:

* The class encodes a House-Haunters-specific rule (e.g. spawn-room
  picking, clue tiers).

If in doubt, start in `game/` and lift it here once a second use site
appears.

## Conventions

* All classes inherit from or compose `GameObject` (defined in
  [include/engine/GameObject.hpp](../engine/GameObject.hpp)) so they
  participate in the engine's update / draw loop.
* No global state — pass dependencies through constructors or `set*`
  methods so tests can wire fakes.
