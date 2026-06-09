# src/game/rooms/

Implementations for the procedural mansion declared in
[include/game/rooms/](../../../include/game/rooms/).

## Contents

| File | Header | Notes |
|---|---|---|
| `Room.cpp` | [Room.hpp](../../../include/game/rooms/Room.hpp) | A single room tile. Sets its background sprite from a PNG under [resources/roompng/](../../../resources/roompng/) and exposes its hitbox / `room_setup` string for upstream queries. |
| `RoomGroup.cpp` | [RoomGroup.hpp](../../../include/game/rooms/RoomGroup.hpp) | Owning collection of rooms plus the spatial queries (`getRoom(index)`, `getRoom(hbox)`, `isInsideRoom(rect)`, `roomCount()`). Built by `GameplayScreen::init` from a procedurally-chosen list of room types laid out on a grid. |

## Layout constants

All "how far apart are rooms" magic numbers live in
[include/engine/Constants.hpp](../../../include/engine/Constants.hpp)
(`kRoomWidth`, `kRoomHeight`, `kDoorSize`, `kRoomGridStrideX`,
`kRoomGridStrideY`). If a room art change requires nudging these, change
the header — not a copy here.
