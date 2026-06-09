# cmake/

CMake helper modules that get added to `CMAKE_MODULE_PATH` from
[CMakeLists.txt](../CMakeLists.txt) so the rest of the build can use
`find_package(...)` against them.

## Contents

| File | Purpose |
|---|---|
| `FindSFML.cmake` | Locates SFML 2.x. Honors `SFML_ROOT` and (in this repo) the vendored copy at [vendor/sfml-extract/SFML-2.6.2/](../vendor/sfml-extract/) so a fresh clone builds without a system SFML install. Sets `SFML_FOUND`, `SFML_INCLUDE_DIR`, and `SFML_LIBRARIES` for the chosen components (`graphics window audio network system`). |

## Adding new modules

Drop a `FindFoo.cmake` here and any `find_package(Foo)` call in the
top-level `CMakeLists.txt` will pick it up. Prefer this directory over
modifying `CMAKE_MODULE_PATH` from elsewhere — keeping every project-local
module under one path makes the dependency picture obvious.

## Not for

* Generated CMake files (those live under `build*/CMakeFiles/`).
* Project-specific build logic — that belongs in the top-level
  [CMakeLists.txt](../CMakeLists.txt) or in [tests/CMakeLists.txt](../tests/CMakeLists.txt).
