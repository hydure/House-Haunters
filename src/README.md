# src/

Implementations for every public header in [include/](../include/). The
folder layout here mirrors `include/` one-for-one so the impl for
`include/foo/Bar.hpp` lives at `src/foo/Bar.cpp`.

`src/` is compiled into the static library `HouseHaunters_core` (see the
`add_library(HouseHaunters_core STATIC ${SRC_FILES})` line in
[CMakeLists.txt](../CMakeLists.txt)). Both the game executable and every
test executable link against that library, so a single change here is
picked up by both flows.

## Subdirectory map

| Folder | What's in it |
|---|---|
| [components/](components/README.md) | Generic reusable game-object building blocks. |
| [engine/](engine/README.md) | Engine internals: event bus, screen stack, input, resource cache, networking, RNG, mod loader. |
| [game/](game/README.md) | House-Haunters-specific code: characters, rooms, screens, objects, `Config`. |

## Top-level implementation

| File | Purpose |
|---|---|
| `HouseHaunters.cpp` | Implements `HouseHauntersGame::init` from [include/HouseHaunters.hpp](../include/HouseHaunters.hpp). Constructs the shared `Config`, loads the mod XML through `ModConfig::loadFromFile(Paths::resource("mods.xml"))`, instantiates every `GameScreen`, and registers them with `addGameScreen(name, ...)`. |

## Adding a new source file

CMake's `file(GLOB_RECURSE SRC_FILES "src/*.cpp")` (with
`CONFIGURE_DEPENDS`) picks up new files on the next build; no edits to
`CMakeLists.txt` needed for ordinary additions. Drop the header in
[include/](../include/) under the matching path and the impl here.

## Conventions

* One `.cpp` per public class. Helper free functions / inline lambdas
  live in anonymous namespaces inside their cpp to keep linker symbols
  clean.
* Includes ordered: own header first, then standard library, then SFML,
  then project headers (alphabetical within each group).
* No `using namespace` at file scope.
* When adding logging, prefer `std::cerr` for hard errors and the
  engine event bus for anything UI-facing.
