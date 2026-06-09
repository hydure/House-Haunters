///////////////////////////
// Hitbox.hpp
//
// A basic axis-aligned bounding box with optional debug visualization.
// Inherits from sf::FloatRect so callers can use the standard intersection
// helpers (intersects, contains, equality, ...). A hitbox can be told to
// follow a GameObject; each update reapplies its initial offset to the
// tracked object's position.
//
///////////////////////////

#ifndef HITBOX_HPP
#define HITBOX_HPP

#include <SFML/Graphics.hpp>
#include "engine/GameObject.hpp"

class Hitbox : public sf::FloatRect, public sf::Drawable
{
public:
    Hitbox() = default;
    Hitbox(float x, float y, float w, float h) : sf::FloatRect(x, y, w, h) {}

    void init();
    void onUpdate(float dt);

    void setDebugMode(bool mode) { isDebugMode = mode; }
    void follow(GameObject* o) { tracker = o; }
    void setColor(sf::Color color) { shape.setOutlineColor(color); }

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

private:
    sf::Vector2f offset;
    sf::RectangleShape shape;
    GameObject* tracker = nullptr;
    bool isDebugMode = false;
};

#endif
