#include "game/rooms/RoomGroup.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include "engine/RandomUtil.hpp"
#include "engine/Constants.hpp"

namespace {
constexpr int kHouseWidth  = 20;
constexpr int kHouseHeight = 20;

// Helper: is `inner` fully contained within `outer`?
bool isContained(const sf::FloatRect& inner, const sf::FloatRect& outer)
{
    return (inner.top  >= outer.top  && inner.top  + inner.height <= outer.top  + outer.height)
        && (inner.left >= outer.left && inner.left + inner.width  <= outer.left + outer.width);
}
} // namespace

RoomGroup::Zone RoomGroup::zoneForOffset(int dx, int dy)
{
    if (std::abs(dx) + std::abs(dy) <= 2) return Zone::HEART;
    if (dy > 0 && std::abs(dy) >= std::abs(dx)) return Zone::CELLAR;
    if (dx < 0) return Zone::SERVICE;
    return Zone::QUARTERS;
}

int RoomGroup::roomTypeFor(Zone zone, int roll)
{
    static const std::array<int, 4> heart = {{2, 4, 5, 7}};
    static const std::array<int, 4> service = {{1, 6, 8, 4}};
    static const std::array<int, 4> quarters = {{10, 11, 12, 5}};
    static const std::array<int, 4> cellar = {{3, 8, 9, 1}};
    const int index = ((roll % 4) + 4) % 4;
    switch (zone) {
        case Zone::HEART:    return heart[static_cast<std::size_t>(index)];
        case Zone::SERVICE:  return service[static_cast<std::size_t>(index)];
        case Zone::QUARTERS: return quarters[static_cast<std::size_t>(index)];
        case Zone::CELLAR:   return cellar[static_cast<std::size_t>(index)];
    }
    return 2;
}

void RoomGroup::generateRoomGrid(int roomCount)
{
    roomCount = std::max(1, std::min(roomCount, kHouseWidth * kHouseHeight));
    totalRooms = roomCount;
    // Clear any rooms left over from a previous game. Without this a second
    // game would accumulate rooms (and their stale cluesInRoom pointers)
    // on top of the previous house.
    this->rooms.clear();
    this->num_rooms = 0;

    // -1 = unused, 0 = candidate, 1 = placed room
    std::array<std::array<int, kHouseHeight>, kHouseWidth> roomGrid{};
    std::array<std::array<int, kHouseHeight>, kHouseWidth> roomTypes{};
    for (auto& col : roomGrid) {
        col.fill(-1);
    }

    const int cx = (kHouseWidth  - 1) / 2;
    const int cy = (kHouseHeight - 1) / 2;
    // Seed the center and its 4-neighborhood as candidates.
    roomGrid[cx][cy]     = 1;
    roomGrid[cx - 1][cy] = 0;
    roomGrid[cx + 1][cy] = 0;
    roomGrid[cx][cy - 1] = 0;
    roomGrid[cx][cy + 1] = 0;

    int roomsGenerated = 1;
    while (roomsGenerated < roomCount) {
        int x = randomInt(kHouseWidth);
        int y = randomInt(kHouseHeight);
        if (roomGrid[x][y] != 0) continue;

        roomGrid[x][y] = 1;
        if (x != 0                  && roomGrid[x - 1][y] != 1) roomGrid[x - 1][y] = 0;
        if (x != kHouseWidth  - 1   && roomGrid[x + 1][y] != 1) roomGrid[x + 1][y] = 0;
        if (y != 0                  && roomGrid[x][y - 1] != 1) roomGrid[x][y - 1] = 0;
        if (y != kHouseHeight - 1   && roomGrid[x][y + 1] != 1) roomGrid[x][y + 1] = 0;
        roomsGenerated++;
    }

    for (int i = 0; i < kHouseWidth; i++) {
        for (int j = 0; j < kHouseHeight; j++) {
            if (roomGrid[i][j] != 1) continue;

            auto currRoom = std::make_unique<Room>();
            currRoom->rect.setSize(sf::Vector2f(EngineConstants::kRoomWidth,
                                                EngineConstants::kRoomHeight));
            currRoom->rect.setPosition(
                static_cast<float>(EngineConstants::kRoomGridStrideX * i),
                static_cast<float>(EngineConstants::kRoomGridStrideY * j));
            const Zone zone = zoneForOffset(i - cx, j - cy);
            const int roll = randomInt(4);
            int roomType = (i == cx && j == cy) ? 2 : roomTypeFor(zone, roll);
            for (int offset = 0; offset < 4 && !(i == cx && j == cy); ++offset) {
                const int candidate = roomTypeFor(zone, roll + offset);
                const bool repeatsLeft = i > 0 && roomTypes[i - 1][j] == candidate;
                const bool repeatsAbove = j > 0 && roomTypes[i][j - 1] == candidate;
                if (!repeatsLeft && !repeatsAbove) {
                    roomType = candidate;
                    break;
                }
            }
            roomTypes[i][j] = roomType;
            currRoom->setRoomType(roomType);
            currRoom->isDoor = false;
            currRoom->setPosition(currRoom->rect.getPosition());
            currRoom->init();

            // Doors extend roughly one character-hitbox into each neighbor.
            if (i + 1 < kHouseWidth && roomGrid[i + 1][j] == 1) {
                auto rightDoor = std::make_unique<Room>();
                rightDoor->rect.setSize(sf::Vector2f(EngineConstants::kDoorSize,
                                                     EngineConstants::kDoorSize));
                rightDoor->rect.setPosition(
                    currRoom->getPosition().x + EngineConstants::kRoomWidth - EngineConstants::kDoorSize,
                    currRoom->getPosition().y + (EngineConstants::kRoomHeight / 2) - EngineConstants::kTileSize);
                rightDoor->setPosition(currRoom->rect.getPosition());
                rightDoor->isDoor = true;
                rightDoor->init();
                this->rooms.push_back(std::move(rightDoor));
            }
            if (j + 1 < kHouseHeight && roomGrid[i][j + 1] == 1) {
                auto bottomDoor = std::make_unique<Room>();
                bottomDoor->rect.setSize(sf::Vector2f(EngineConstants::kDoorSize,
                                                      EngineConstants::kDoorSize));
                bottomDoor->rect.setPosition(
                    currRoom->getPosition().x + (EngineConstants::kRoomWidth / 2) - EngineConstants::kTileSize,
                    currRoom->getPosition().y + EngineConstants::kRoomHeight - EngineConstants::kDoorSize);
                bottomDoor->setPosition(currRoom->rect.getPosition());
                bottomDoor->isDoor = true;
                bottomDoor->isBottom = true;
                bottomDoor->init();
                this->rooms.push_back(std::move(bottomDoor));
            }

            this->rooms.push_back(std::move(currRoom));
        }
    }
}

// True if `hbox` is fully inside any room rectangle.
bool RoomGroup::isInsideRoom(sf::FloatRect hbox)
{
    for (const auto& r : rooms) {
        if (isContained(hbox, r->hbox)) {
            return true;
        }
    }
    return false;
}

// Returns the FloatRect of the (non-door) room containing `hbox`, or an
// empty rect if none does.
sf::FloatRect RoomGroup::getRoom(sf::FloatRect hbox)
{
    for (const auto& r : rooms) {
        if (r->isDoor) continue;
        if (isContained(hbox, r->hbox)) {
            return r->hbox;
        }
    }
    return sf::FloatRect{};
}

// Returns the n-th non-door room, or nullptr if `room_num` is out of range.
Room* RoomGroup::getRoom(int room_num)
{
    int num = 0;
    for (const auto& r : rooms) {
        if (r->isDoor) continue;
        if (room_num == num++) {
            return r.get();
        }
    }
    return nullptr;
}

int RoomGroup::roomCount()
{
    if (this->num_rooms == 0) {
        for (const auto& r : rooms) {
            if (!r->isDoor) this->num_rooms++;
        }
    }
    return this->num_rooms;
}

bool RoomGroup::inSameRoom(sf::FloatRect box1, sf::FloatRect box2)
{
    return getRoomInside(box1) == getRoomInside(box2);
}

Room* RoomGroup::getRoomInside(sf::FloatRect hbox)
{
    for (const auto& r : rooms) {
        if (r->isDoor) continue;
        if (isContained(hbox, r->hbox)) {
            return r.get();
        }
    }
    return nullptr;
}

void RoomGroup::onDraw(sf::RenderTarget& target, sf::RenderStates /*states*/) const
{
    for (const auto& r : rooms) {
        if (r->isDoor) {
            target.draw(*r);
        }
    }
}
