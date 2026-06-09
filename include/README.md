# include/

Public C++ headers for the project. Mirrors the layout of [src/](../src/)
one-for-one: a header at `include/foo/Bar.hpp` has its implementation at
`src/foo/Bar.cpp`.

Everything under `include/` is on the public include path for both the
game executable and the test suite (see the `target_include_directories`
call in [CMakeLists.txt](../CMakeLists.txt)), so headers can `#include`
each other without relative paths:

```cpp
#include "engine/EventManager.hpp"
#include "game/characters/Character.hpp"
```

## Subdirectory map

| Folder | What's in it |
|---|---|
| [components/](components/README.md) | Generic reusable game-object building blocks (sprite animation, hitbox, entity group). Nothing here knows about House Haunters specifically. |
| [engine/](engine/README.md) | Engine internals shared across screens: event bus, screen stack, input, resource cache, networking, RNG, mod loader. |
| [game/](game/README.md) | House-Haunters-specific code: characters, villains, rooms, screens, `Config`. |
| [rapidxml/](rapidxml/README.md) | Vendored single-header XML parser. |

## Top-level header

| File | Purpose |
|---|---|
| `HouseHaunters.hpp` | `HouseHauntersGame` — the `GameEngine` subclass that wires every screen together. Implemented in [src/HouseHaunters.cpp](../src/HouseHaunters.cpp). |

## Conventions

* One class per header where practical; small helper structs may share a
  header with their primary class.
* Header guards use `#ifndef SHOUTY_FILE_NAME_HPP` (no `#pragma once` —
  keeps MSVC and GCC behavior identical on case-insensitive filesystems).
* No `using namespace` in headers.
* No SFML window/graphics objects as static globals — engine state lives
  on `GameEngine` and per-screen state on the active `GameScreen`.
