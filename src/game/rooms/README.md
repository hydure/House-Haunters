# src/game/rooms/

Implementations for the procedural mansion declared in
[include/game/rooms/](../../../include/game/rooms/).

## Contents

| File | Header | Notes |
|---|---|---|
| `Room.cpp` | [Room.hpp](../../../include/game/rooms/Room.hpp) | A single room tile. Sets its background sprite from a PNG under [resources/roompng/](../../../resources/roompng/) and exposes its hitbox / `room_setup` string for upstream queries. |
| `RoomGroup.cpp` | [RoomGroup.hpp](../../../include/game/rooms/RoomGroup.hpp) | Owning collection and spatial queries. The connected grid is divided into a throne-centered heart, service wing, private quarters, and cellar band; each zone draws from a themed room pool and avoids immediate visual repeats. |

## Layout constants

All "how far apart are rooms" magic numbers live in
[include/engine/Constants.hpp](../../../include/engine/Constants.hpp)
(`kRoomWidth`, `kRoomHeight`, `kDoorSize`, `kRoomGridStrideX`,
`kRoomGridStrideY`). If a room art change requires nudging these, change
the header — not a copy here.
