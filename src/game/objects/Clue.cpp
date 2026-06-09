#include "game/objects/Clue.hpp"

void Clue::init()
{
    this->setPosition(static_cast<float>(xPos), static_cast<float>(yPos));

    hbox = Hitbox(static_cast<float>(xPos),
                  static_cast<float>(yPos),
                  static_cast<float>(width),
                  static_cast<float>(height));
    hbox.follow(this);
    hbox.init();
    this->hbox.setColor(sf::Color::Yellow);

    isOpen = false;
}

void Clue::setCoordinates(int x, int y, int w, int h)
{
    this->xPos = x;
    this->yPos = y;
    this->width = w;
    this->height = h;
}

void Clue::onUpdate(float dt)
{
    hbox.onUpdate(dt);
}

void Clue::onDraw(sf::RenderTarget& target, sf::RenderStates states) const
{
    target.draw(sprite, states);
    target.draw(hbox);
}

void Clue::open()  { isOpen = true; }
void Clue::close() { isOpen = false; }
