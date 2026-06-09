# resources/

All assets shipped with the game: PNGs, fonts, audio, Tiled `.tmx` / `.tsx`
sources, plus the data-driven config files for clues and mods. Anything
the game reads at runtime lives here; anything generated at build time
does not.

At runtime, asset paths are resolved relative to the executable as
`../resources/<rel>` by [Paths::resource()](../include/engine/Paths.hpp).
This is why the release binary lives at `build-release/HH.exe` and not at
the repo root.

## Top-level files

| File | Purpose |
|---|---|
| `mods.xml` | Data-driven character / villain / lobby / audio configuration. See [MODDING.md](../MODDING.md). |
| `items.xml` | Master clue list — items the ghost can be killed with and the clue strings revealed for each tier. Parsed by [ClueReader](../include/engine/ClueReader.hpp). |
| `items.json` | Companion to `items.xml` (legacy / reference). Not read at runtime. |
| `HH_Portraits.png` | Atlas of character portraits drawn on the lobby screen. Layout is described in `mods.xml` (`<screen portrait_atlas=... tile_w=... tile_h=... columns=.../>`). |
| `HH_Portraits.png.orig` | Pristine pre-edit copy of the portrait atlas. |
| `HH_Portraits.psd` | Photoshop source for the portrait atlas. |
| `character.png` | Combined sprite sheet for the four playable characters (also referenced by `mods.xml` via `<character ... sprite_sheet="..."/>`). |
| `HH_Mom.png` | Mother sprite variant. |
| `Ghost.png` | Default villain spritesheet. Mod-overridable via the `<villain sprite="..."/>` entry in `mods.xml`. |
| `titlescreen.png`, `titlescreen.png.orig` | Title screen background. |
| `storyscreen.png` | Background for the opening story scroll. |
| `hh.png`, `HH_Black_*.png`, `HH_White_*.png` | Logo variants used on title / end-game screens. |
| `Interior_Sprites.psd` | Photoshop source for the interior tilesets in [sprites/](sprites/). |

## Subfolders

| Folder | Contents |
|---|---|
| [fonts/](fonts/README.md) | TrueType fonts loaded by [ResourceManager::getFont](../include/engine/ResourceManager.hpp). |
| [music/](music/README.md) | Looping music + situational audio cues. |
| [room/](room/README.md) | Tiled `.tmx` map sources + `.tsx` tilesets. Editor-only — the runtime reads pre-rendered PNGs from [roompng/](roompng/). |
| [roompng/](roompng/README.md) | Pre-rendered PNGs for each room layout. These are what the engine loads at runtime. |
| [shaders/](shaders/README.md) | GLSL source for visual effects. |
| [sound/](sound/README.md) | One-shot sound effects (placeholder). |
| [sprites/](sprites/README.md) | Character / world spritesheets + tilesets. |

## Editing assets

* PNGs are loaded via SFML's image decoder — any tool that produces a
  PNG with an alpha channel works. The PSDs are checked in as the
  editable source of truth where one exists.
* `.tmx` / `.tsx` files come from [Tiled](https://www.mapeditor.org/). They
  are not loaded at runtime — they're the source you edit before
  exporting each updated `roompng/room_*.png`. Keep both in sync.
* Audio formats: `.ogg`, `.flac`, and `.wav` are all valid (SFML uses
  libsndfile). Prefer `.ogg` for long loops, `.wav` for short cues.
* XML and JSON files are plain text — `mods.xml` and `items.xml` ship
  with comments describing their schema.

## Not for

* Generated atlases (the script in [tools/build_atlas.py](../tools/build_atlas.py)
  re-emits `character_sheet.png` and the portrait atlas; the
  pre-generated outputs are checked in here as a convenience).
* Compiled / encrypted formats — every asset here is openable in a
  standard editor.
