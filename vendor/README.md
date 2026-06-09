# vendor/

Pinned third-party dependencies, checked into the repo so a fresh clone
builds without external `pip install` / `apt` / `vcpkg` steps. Nothing
here is authored by this project; do not edit files under `vendor/` —
upstream updates should replace the whole subdirectory.

## Contents

| Entry | What it is |
|---|---|
| `sfml-extract/` | SFML 2.6.2, extracted from the official release zip. Discovered by [cmake/FindSFML.cmake](../cmake/FindSFML.cmake) so `find_package(SFML)` works without a system install. |
| `sfml.zip` | The original release archive; kept so the extraction can be reproduced. |
| `python/` | Windows embeddable Python distribution. Used to run [tools/build_atlas.py](../tools/build_atlas.py) on machines without a system Python install (and by [tests/test_build_atlas.py](../tests/test_build_atlas.py)). |
| `python-embed.zip` | Original Python distribution archive. |

## Adding a new dependency

1. Drop the upstream archive here (`vendor/<name>.zip` or `.tar.gz`) so
   the version is auditable.
2. Extract into `vendor/<name>-extract/` (or however the upstream
   layout demands).
3. Add any required CMake glue to [cmake/](../cmake/) — never modify the
   vendored files.
4. Document the version in this README and update the project root
   [README.md](../README.md) if the new dep is user-facing.

## Why not git submodules?

Submodules require a network round-trip on every fresh clone and add a
moving target to bisection. A pinned, in-tree drop is reproducible and
hermetic.

## Licensing

Each subdirectory keeps its upstream license file (`license.md`,
`LICENSE.txt`, etc.). Consult those before redistributing anything from
`vendor/`.
