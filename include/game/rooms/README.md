# include/game/rooms/

The mansion's floor plan. Each `Room` is one tile of the procedurally
laid-out house; `RoomGroup` owns the whole collection and provides spatial
queries (which room is at this position, which rooms are doors, what room
is the character in right now).

## Contents

| Header | Purpose |
|---|---|
| `Room.hpp` | A single room. Holds the room's tile background, axis-aligned bounding box (`sf::FloatRect hbox`), a `room_setup` string identifying its layout variant (e.g. `parlor`, `grave`, `wood_bedroom`), and an `isDoor` flag for transition tiles. |
| `RoomGroup.hpp` | Owning vector of rooms (`std::vector<std::unique_ptr<Room>> rooms`) plus the spatial-query helpers — `getRoom(index)`, `getRoom(hbox)`, `isInsideRoom(rect)`, `roomCount()`. Built once at the start of `GameplayScreen::init` by procedurally placing room PNG tiles on a 2D grid. |

## Coordinate conventions

* Room art is 512×384 px (`EngineConstants::kRoomWidth` ×
  `kRoomHeight`).
* Door cutouts are 64 px on each wall (`kDoorSize`).
* Adjacent rooms are placed at a stride of `kRoomGridStrideX = 448` and
  `kRoomGridStrideY = 294` so the wall pixels overlap visually but the
  hitboxes line up. Constants live in [include/engine/Constants.hpp](../../engine/Constants.hpp).

## Adding a new room layout

1. Drop the room PNG into [resources/roompng/](../../../resources/roompng/).
2. Optionally drop a Tiled `.tmx` editor file into
   [resources/room/](../../../resources/room/) for reproducibility.
3. Add the `room_setup` string to the spawn-offset table in
   `src/game/characters/Character.cpp` (the `kSpawnY` map in
   `spawnYOffsetFor`) so characters spawn at sensible heights inside it.
4. Cover any new placement rules with a test under [tests/](../../../tests/).
