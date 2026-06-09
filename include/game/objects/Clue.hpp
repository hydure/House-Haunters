#ifndef CLUES_HPP
#define CLUES_HPP

#include <memory>
#include <string>
#include <SFML/Graphics.hpp>
#include "engine/Engine.hpp"
#include "engine/ClueReader.hpp"
#include "components/Hitbox.hpp"
#include "game/rooms/Room.hpp"
#include "game/rooms/RoomGroup.hpp"
#include "components/EntityGroup.hpp"

////////////////
// Clue.hpp
//
// A collectible/searchable that lives in a room. Players walk over it to
// reveal a clue string. Clues belong to either the high- or low-damage
// item set selected at gameplay start.
////////////////

class Clue : public GameObject
{
public:
    void init() override;
    void onUpdate(float dt) override;
    void setCoordinates(int x, int y, int w, int h);
    void onDraw(sf::RenderTarget& target, sf::RenderStates states) const override;
    void open();
    void close();
    void setRoomGroup(RoomGroup* group) { g = group; }
    void setEntities(EntityGroup* entities) { entity_group = entities; }
    void setClueNumber(int number) { clue_number = number; }

    Hitbox hbox;
    bool isOpen = false;
    int highLow = 0;
    bool activatedItem = false;

    // The written information shown to the player
    std::string clueSpec;
    std::string clueVague;
    std::string clueWorthless;
    std::string clueJackpot;
    std::string setClue;
    int clue_number = -1;

protected:
    int xPos = 0;
    int yPos = 0;
    int width = 0;
    int height = 0;
    RoomGroup* g = nullptr;
    EntityGroup* entity_group = nullptr;
    sf::Sprite sprite;
};

#endif
