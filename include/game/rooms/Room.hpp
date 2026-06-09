#ifndef ROOM_HPP
#define ROOM_HPP

#include <string>
#include <vector>
#include <SFML/Graphics.hpp>
#include "engine/Engine.hpp"
#include "components/Hitbox.hpp"

// Forward declare to avoid a circular include with Clue.hpp -- Clue already
// includes Room.hpp, and we only need the pointer type here.
class Clue;

class Room : public GameObject
{
public:
    void init() override;
    void onDraw(sf::RenderTarget& target, sf::RenderStates states) const override;

    // Configure this room from one of the predefined type ids (1..12).
    // Sets `room_setup` (string identifier) and populates `clueCoordinates`
    // with one rect per clue, expressed in tile-grid units (1 tile = 32 px).
    void setRoomType(int type);

    sf::RectangleShape rect;
    sf::Sprite room_sprite;
    std::vector<sf::IntRect> clueCoordinates;
    std::string room_setup;
    Hitbox hbox;
    bool isDoor = false;
    bool isBottom = false;

    // Spatial index: every clue that physically sits inside this room.
    // Populated by GameplayScreen::createClues after the EntityGroup has
    // taken ownership; these are non-owning observers (clue lifetime is
    // managed by EntityGroup). Character::checkClues iterates this list
    // rather than every clue in the house.
    std::vector<Clue*> cluesInRoom;
};

#endif
