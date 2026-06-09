# resources/roompng/

Pre-rendered PNGs for every room layout. The engine loads these directly
at runtime; the [Tiled](https://www.mapeditor.org/) `.tmx` sources in
[../room/](../room/) are the editable source of truth.

Each room is 512×384 px and pre-cut for 64-px door openings on each
wall (the constants live in
[include/engine/Constants.hpp](../../include/engine/Constants.hpp)).

## Contents

| File | Notes |
|---|---|
| `room_1.png` … `room_12.png` | One render per room layout. The numbering corresponds to room indices used by [RoomGroup](../../include/game/rooms/RoomGroup.hpp) when assembling a level. |
| `view/` | Working folder for design references (preview screenshots etc.); not loaded at runtime. |

## Editing workflow

1. Open the matching `.tmx` in [../room/](../room/) under Tiled.
2. Edit and re-export to PNG.
3. Save over the `room_N.png` here.

## Adding a new room

Add the next `room_<N+1>.png` here, place the matching `.tmx` source in
[../room/](../room/), and update the room-pool selection logic in
[src/game/screens/GameplayScreen.cpp](../../src/game/screens/GameplayScreen.cpp)
so the new layout actually gets picked.

## Naming

Stick with `room_<int>.png` (lowercase, underscore, integer). The engine
expects an integer suffix when iterating the room set.
