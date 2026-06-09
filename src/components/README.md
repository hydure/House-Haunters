# src/components/

Implementations for the generic reusable building blocks declared in
[include/components/](../../include/components/). See that folder's
[README](../../include/components/README.md) for the responsibility of
each class.

## Contents

| File | Header | Notes |
|---|---|---|
| `EntityGroup.cpp` | [EntityGroup.hpp](../../include/components/EntityGroup.hpp) | Owns `Character*` storage in a flat vector; `getCharacter(slot)` is a linear scan keyed on `Character::playerNumber`. |
| `Hitbox.cpp` | [Hitbox.hpp](../../include/components/Hitbox.hpp) | Pure math + a parent pointer to follow; no SFML rendering happens here. Cheap to test. |
| `SpriteAnimation.cpp` | [SpriteAnimation.hpp](../../include/components/SpriteAnimation.hpp) | Frame-time accumulator + index advancement. Pulls `sf::Texture` references from [ResourceManager](../../include/engine/ResourceManager.hpp) so multiple animations sharing a spritesheet only load once. |

## Conventions

Everything here must remain game-agnostic. If a change here requires
including a header from [src/game/](../game/) or
[include/game/](../../include/game/), it belongs in `src/game/` instead.
