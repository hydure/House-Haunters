# tools/

Out-of-band helper scripts run by developers — never invoked by the game
itself at runtime. Anything here is optional; the game ships with the
finished artifacts these scripts produce, not the scripts.

## Contents

| File | Purpose |
|---|---|
| `build_atlas.py` | Packs a set of source sprite PNGs into a single texture atlas plus a JSON manifest. Used to regenerate `resources/sprites/character_sheet.png` and the portrait atlas when the source art changes. |
| `atlas_manifest.json` | The manifest produced by `build_atlas.py` describing tile sizes, source files, and atlas coordinates. Checked in so anyone can see how the current atlas was assembled. |
| `__pycache__/` | Python bytecode cache. Safe to delete; regenerated on next run. |

## Running

`build_atlas.py` requires Python 3 and [Pillow](https://pillow.readthedocs.io/).
Install with:

```powershell
pip install Pillow
```

Then re-pack from the project root:

```powershell
python tools\build_atlas.py
```

A regression test exists at [tests/test_build_atlas.py](../tests/test_build_atlas.py)
and is auto-registered with CTest when a `python3` interpreter is on
`PATH` (otherwise it's skipped silently — there's no hard Python
dependency in the C++ build).

## Adding new tools

Drop a script here, document it in this README, and (if it's worth
guarding against regressions) add a `tests/test_<name>.py` that CTest can
pick up.

## Not for

* C++ build tooling — that belongs in [cmake/](../cmake/) or
  [CMakeLists.txt](../CMakeLists.txt).
* Distributable artifacts — those go in `dist/` (currently a placeholder).
