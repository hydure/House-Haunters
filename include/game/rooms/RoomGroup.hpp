#ifndef ROOMGROUP_HPP
#define ROOMGROUP_HPP

#include <memory>
#include <vector>
#include "game/rooms/Room.hpp"
#include "components/Hitbox.hpp"

/**
 * Owns the set of Rooms generated for the current game. Rooms live in
 * the `rooms` vector (NOT as GameObject::children) so the spatial
 * lookups (getRoomInside, inSameRoom, ...) can scan a tight, typed
 * vector instead of the generic scene-graph list. RoomGroup is still a
 * GameObject so its onDraw can render every room in z-order with one
 * polymorphic call from the gameplay screen.
 */
class RoomGroup : public GameObject
{
public:
    enum class Zone {
        HEART,
        SERVICE,
        QUARTERS,
        CELLAR
    };

    void generateRoomGrid(int roomCount);
    static Zone zoneForOffset(int dx, int dy);
    static int roomTypeFor(Zone zone, int roll);
    bool isInsideRoom(sf::FloatRect hbox);
    bool inSameRoom(sf::FloatRect box1, sf::FloatRect box2);
    sf::FloatRect getRoom(sf::FloatRect hbox);
    int totalRooms = 0;
    Room* getRoom(int room_num);
    Room* getRoomInside(sf::FloatRect hbox);
    int roomCount();
    std::vector<std::shared_ptr<Room>> rooms;
protected:
    int num_rooms = 0;
    void onDraw(sf::RenderTarget& target, sf::RenderStates states) const override;
};

#endif
