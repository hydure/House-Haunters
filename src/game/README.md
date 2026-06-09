# src/game/

Implementations for the game-specific classes declared in
[include/game/](../../include/game/). See that folder's
[README](../../include/game/README.md) for the responsibility of each
class.

## Subdirectory map

| Folder | What's in it |
|---|---|
| [characters/](characters/README.md) | `Character`, `Villain`, `PlayerView`. |
| [objects/](objects/README.md) | `Clue`. |
| [rooms/](rooms/README.md) | `Room`, `RoomGroup`. |
| [screens/](screens/README.md) | One `.cpp` per `GameScreen` subclass. |

## Top-level implementation

| File | Header | Notes |
|---|---|---|
| `Config.cpp` | [Config.hpp](../../include/game/Config.hpp) | Implements the lookup helpers (`Config::getCharacter`, `Config::setCharacter`, etc.) and the difficulty / character enum to string maps. |

`JoinCode.hpp` is header-only (it's a pair of tiny inline encoder /
decoder functions); there's no `JoinCode.cpp`.

## Conventions

* Game-specific code may include from `include/engine/` and
  `include/components/`, but **not** the other way around. If a useful
  abstraction emerges, lift the relevant class up into one of the lower
  layers.
* Don't talk to `sf::RenderWindow` directly from here — go through
  `GameEngine`. Tests do not have a window.
