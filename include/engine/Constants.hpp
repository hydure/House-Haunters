#ifndef ENGINE_CONSTANTS_HPP
#define ENGINE_CONSTANTS_HPP

////////////////////////////////
// Constants.hpp
//
// Engine-wide magic numbers extracted to one place. Most of these used
// to be sprinkled across Room/Character/Villain/RoomGroup. If the art
// changes, edit here.
////////////////////////////////

namespace EngineConstants
{
    // One tile in the room art = 32 px square. Used for clue placement,
    // spawn offsets, hitbox sizes, ...
    constexpr int kTileSize = 32;

    // Visual size of one room sprite (the .png we draw underneath).
    constexpr int kRoomWidth  = 512;
    constexpr int kRoomHeight = 384;

    // Doorway size between adjacent rooms.
    constexpr int kDoorSize = 64;

    // Walkable interior bounds (room minus walls / door cutouts).
    constexpr int kRoomInteriorWidth  = kRoomWidth  - kDoorSize;     // 448
    constexpr int kRoomInteriorHeight = kRoomHeight - 96;            // 288

    // Stride between adjacent rooms when laid out in a grid.
    constexpr int kRoomGridStrideX = kRoomWidth  - kDoorSize;        // 448
    constexpr int kRoomGridStrideY = kRoomHeight - 90;               // 294

    // Default window size (also stored on Config so it can be overridden).
    constexpr int kWindowWidth  = 720;
    constexpr int kWindowHeight = 480;

    // Alpha units per second used by the fade-from-black transitions on
    // story / title / character / end screens.
    constexpr float kFadeRate = 60.0f;
}

#endif
