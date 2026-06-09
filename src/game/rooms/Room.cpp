#include "game/rooms/Room.hpp"

#include <array>
#include <string>
#include <vector>

#include "engine/Paths.hpp"

namespace {

// Static description of a room "type": its string identifier (used by
// Character.cpp for spawn placement) and the list of clue rectangles
// that get appended to Room::clueCoordinates. Rects are in tile-grid
// units (1 tile = 32 px).
struct RoomDef {
    const char* name;
    std::vector<sf::IntRect> clues;
};

// Indexed by room type - 1. Room types are 1..12.
const std::array<RoomDef, 12>& roomTable()
{
    static const std::array<RoomDef, 12> table = {{
        // 1: armory
        { "armory", {
            { 1,  2, 1, 1},  // spears
            { 2,  8, 1, 2},  // hay
            { 4,  3, 1, 1},  // chest
            { 6,  7, 1, 1},  // table
            {11,  7, 1, 2},  // bookshelf
            {11,  3, 1, 1},  // chest
            {14,  2, 1, 2},  // chest
        }},
        // 2: throne
        { "throne", {
            { 7,  3, 2, 2},  // queen
            { 7,  5, 2, 2},  // pedestal
            { 2,  2, 1, 1},  // column
            { 2,  8, 1, 3},  // column
            {13,  2, 1, 1},  // column
            {13,  8, 1, 3},  // column
        }},
        // 3: grave
        { "grave", {
            { 1,  2, 2, 2},  // stump
            { 3,  7, 1, 3},  // leafy column
            {11,  7, 1, 3},  // column
            { 7,  5, 1, 2},  // grave
            {12,  3, 2, 2},  // rock
        }},
        // 4: parlor
        { "parlor", {
            { 1,  3, 1, 2},  // chest
            { 3,  2, 2, 1},  // dresser
            { 2,  7, 3, 3},  // table
            { 6,  2, 1, 1},  // plant1
            { 9,  2, 1, 1},  // plant2
            { 6,  9, 1, 2},  // plant3
            { 9,  9, 1, 2},  // plant4
            {11,  3, 2, 2},  // couch
            {11,  7, 3, 3},  // piano
        }},
        // 5: lounge
        { "lounge", {
            { 1,  7, 1, 2},  // candle
            { 3,  2, 3, 1},  // fireplace
            { 8,  5, 2, 2},  // couch
            { 7,  7, 4, 2},  // table
            {11,  2, 2, 1},  // china
        }},
        // 6: kitchen
        { "kitchen", {
            { 1,  2, 5, 1},  // furniture
            { 6,  5, 5, 2},  // table + chairs
            { 1, 11, 4, 1},  // stove/etc
        }},
        // 7: lion
        { "lion", {
            { 5,  2, 1, 1},  // vase
            { 3,  3, 1, 1},  // chair
            { 7,  4, 2, 3},  // lion
            {13,  2, 1, 1},  // clock
        }},
        // 8: barrels
        { "barrels", {
            { 2,  2, 2, 1},  // barrels
            { 5,  5, 1, 2},  // chair left
            { 6,  4, 4, 4},  // table
            {10,  5, 1, 2},  // chair right
            { 6,  2, 1, 1},  // candle1
            { 9,  2, 1, 1},  // candle2
        }},
        // 9: dungeon
        { "dungeon", {
            { 1,  4, 1, 1},  // torch1
            { 1,  7, 1, 2},  // torch2
            {14,  4, 1, 1},  // torch3
            {14,  7, 1, 2},  // torch4
            { 2,  3, 1, 1},  // chest
            { 5,  5, 1, 1},  // cauldron
            { 3, 10, 1, 1},  // bones
            { 9,  6, 3, 1},  // table + chair
            { 9, 10, 1, 1},  // water
            {10,  9, 3, 2},  // bed
        }},
        // 10: bedroom
        { "bedroom", {
            { 1,  8, 1, 2},  // chest
            { 2,  2, 3, 1},  // dresser1
            {10,  2, 4, 1},  // dresser2
            { 7,  5, 2, 3},  // bed
            {10,  6, 1, 1},  // end table
            {13,  7, 1, 3},  // clock
        }},
        // 11: wood_bedroom
        { "wood_bedroom", {
            { 1,  7, 2, 3},  // table
            { 5,  2, 1, 1},  // clock
            {10,  7, 2, 2},  // chair
            {10,  2, 1, 1},  // lamp
            {11,  2, 2, 2},  // bed
        }},
        // 12: bathroom
        { "bathroom", {
            { 1,  3, 1, 2},  // candle1
            { 1,  7, 1, 2},  // candle2
            {14,  3, 1, 2},  // candle3
            {14,  7, 1, 2},  // candle4
            { 4,  4, 8, 5},  // water
            {12,  5, 1, 1},  // pail
        }},
    }};
    return table;
}

} // namespace

void Room::init()
{
    const sf::Vector2f pos  = rect.getPosition();
    const sf::Vector2f size = rect.getSize();

    if (!isDoor) {
        hbox = Hitbox(pos.x + 32, pos.y + 64, size.x - 64, size.y - 96);
    } else {
        hbox = Hitbox(pos.x, pos.y, size.x, size.y);

        if (isBottom) {
            rect.setSize(sf::Vector2f(size.x, size.y / 2));
            rect.move(0, 16);
        } else {
            rect.setSize(sf::Vector2f(size.x / 2, size.y));
            rect.move(16, 0);
        }
        rect.setFillColor(sf::Color::Black);
    }
    hbox.init();
}

void Room::setRoomType(int type)
{
    const auto& table = roomTable();
    const std::size_t idx = static_cast<std::size_t>(type - 1);
    if (idx >= table.size()) {
        return; // unknown type -- leave room_setup empty
    }

    const RoomDef& def = table[idx];
    room_setup = def.name;
    clueCoordinates.insert(clueCoordinates.end(), def.clues.begin(), def.clues.end());

    const std::string location =
        Paths::resource("roompng/room_" + std::to_string(type) + ".png");
    room_sprite.setTexture(*ResourceManager::getTexture(location));
}

void Room::onDraw(sf::RenderTarget& target, sf::RenderStates states) const
{
    target.draw(room_sprite, states);
    if (isDoor) {
        target.draw(rect);
    }
    target.draw(hbox);
}
