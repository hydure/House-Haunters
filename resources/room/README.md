# resources/room/

[Tiled](https://www.mapeditor.org/) source files for the room layouts.
These files are **not** read at runtime — the engine loads the
pre-rendered PNGs from [roompng/](../roompng/). Treat the `.tmx` files
here as the editable "source of truth" for room art and re-export the
PNGs whenever you change one.

## Contents

| File | Type | Purpose |
|---|---|---|
| `armorroom.tmx`, `bathroom.tmx`, `bathroom2.tmx`, `bedroom.tmx`, `bedroom2.tmx`, `creepy.tmx`, `dinning.tmx`, `gallery.tmx`, `kitchen.tmx`, `livingroom.tmx`, `oneroom.tmx`, `outside.tmx`, `prayer.tmx`, `readingroom.tmx` | Tiled map | One room layout per file. Re-export each one to `../roompng/room_N.png` after editing. |
| `Exterior Tiles.tsx`, `Interior 2.tsx`, `Interior1.tsx`, `interior.tsx`, `Terrain and Outside.tsx`, `WaterAndFire.tsx`, `blood.tsx`, `blood2.tsx`, `preview.tsx` | Tiled tileset | The tile palettes used by the `.tmx` maps. Each references a corresponding PNG in [../sprites/](../sprites/) — keep them next to each other in Tiled. |

## Editing workflow

1. Open the `.tmx` in Tiled.
2. Edit. Save.
3. Export the map to PNG (`File -> Export As`).
4. Save the PNG over the matching file in [../roompng/](../roompng/) so
   the engine picks up the change next launch.
5. Commit both the `.tmx` source and the regenerated PNG in the same
   commit so future authors can edit further.

## Adding a new room

* Add the `.tmx` here.
* Add the exported PNG to [../roompng/](../roompng/) as `room_N.png`
  (next free integer).
* Add the room's `room_setup` string to the spawn-offset table in
  [src/game/characters/Character.cpp](../../src/game/characters/Character.cpp)
  so characters spawn at the right Y inside it.
