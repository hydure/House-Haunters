# resources/sprites/

Spritesheets and tilesets used by the characters, the villain, and the
room renderer. PNG with alpha channel, loaded via
[ResourceManager::getTexture](../../include/engine/ResourceManager.hpp).

## Contents

### Character / villain spritesheets

| File | Notes |
|---|---|
| `character_sheet.png` | Default playable-character sheet. Layout (rows × cols, frame size, walk-cycle indices) is described per character in [resources/mods.xml](../mods.xml). Regenerable from `character_sheet.psd` via [tools/build_atlas.py](../../tools/build_atlas.py). |
| `character_sheet.psd` | Photoshop source for the character sheet. |
| `ghost.png` | Default villain sprite. Mod-overridable via the `<villain sprite="..."/>` entry in `mods.xml`. |
| `white_chara.png` | Variant character sheet used by certain mod presets. |
| `rando.png` | Spare / experimental character sheet. |

### Interactable / overlay sprites

| File | Notes |
|---|---|
| `heart.png` | HUD heart icon drawn by [PlayerView](../../include/game/characters/PlayerView.hpp). |
| `scroll.png` | Clue parchment overlay. |
| `swipe.png` | Attack-swing visual. |
| `grave.png` | World-object grave sprite. |
| `blood.png`, `blood2.png` | Damage / death overlays. |

### Tilesets

| File | Used by Tiled tileset |
|---|---|
| `interior.png` | [../room/interior.tsx](../room/interior.tsx) |
| `Interior 2.png` | [../room/Interior 2.tsx](../room/Interior%202.tsx) |
| `Interior1.png` | [../room/Interior1.tsx](../room/Interior1.tsx) |
| `Exterior Tiles.png` | [../room/Exterior Tiles.tsx](../room/Exterior%20Tiles.tsx) |
| `Terrain and Outside.png` | [../room/Terrain and Outside.tsx](../room/Terrain%20and%20Outside.tsx) |
| `WaterAndFire.png` | [../room/WaterAndFire.tsx](../room/WaterAndFire.tsx) |
| `tileset_16x16_interior.png` | Generic 16×16 interior tile palette. |
| `lights.png` | Lighting overlay tiles. |
| `ak's_assets.png` | Third-party asset pack. |
| `preview.png`, `preview.png.orig` | Tiled preview thumbnail. |

## Editing workflow

* Hand-edited PNGs: open in your editor of choice, save back over the
  PNG.
* Sheets with a `.psd` source: edit the PSD and re-export to PNG. If
  the corresponding atlas is regenerable via
  [tools/build_atlas.py](../../tools/build_atlas.py), re-run it after
  editing so the manifest stays in sync.
* When changing a frame layout, update the matching `<character>` or
  `<villain>` entry in [../mods.xml](../mods.xml) (frame size, walk
  cycle indices) so the runtime keeps matching the new art.

## Adding a sprite

* Drop the PNG here.
* Reference it via `Paths::resource("sprites/your_file.png")`.
* For characters / villains, the cleanest path is to add a mod entry
  rather than wiring it into code — see [MODDING.md](../../MODDING.md).
