#include "engine/GameObject.hpp"

// Drives onUpdate for every child, then for self.
void GameObject::update(float dt)
{
    for (const auto& child : children) {
        child->update(dt);
    }
    this->onUpdate(dt);
}

// Applies our transform, draws self, then draws every child.
void GameObject::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    states.transform *= this->getTransform();
    this->onDraw(target, states);
    for (const auto& child : children) {
        target.draw(*child, states);
    }
}

void GameObject::addChild(GameObjectPtr o)
{
    o->setParent(this);
    o->init();
    children.push_back(std::move(o));
}
