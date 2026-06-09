# bin/

Single-file home of the game's `main()` plus any other small driver
executables. CMake auto-discovers everything matching `bin/*.cpp` and
emits one executable target per file (see the `foreach(EXEC ${EXECLIST})`
block in [CMakeLists.txt](../CMakeLists.txt)).

## Contents

| File | Purpose |
|---|---|
| `HH.cpp` | Entry point for the game. Parses CLI flags (`--fullscreen`, `--windowed`, `--help`), forwards them through environment variables, and hands control to `HouseHauntersGame::init()` from [include/HouseHaunters.hpp](../include/HouseHaunters.hpp). |

## Adding a new executable

Drop a `something.cpp` here with its own `int main(...)` and re-run
CMake (CONFIGURE_DEPENDS makes incremental rebuilds notice the new
file). The new target will automatically link against `hh_compile_options`
(the shared warning flags) and, if the file uses any of the engine, also
against `HouseHaunters_core`.

## Conventions

* Keep these files thin: pull anything substantive into `src/` so the
  test suite can link against it via `HouseHaunters_core`.
* CLI flag handling that needs to be visible to other systems (e.g.
  fullscreen, network mode) should round-trip through env vars so the
  `Env::` helper in [include/engine/Env.hpp](../include/engine/Env.hpp)
  becomes the single source of truth.
