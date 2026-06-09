#include "components/EntityGroup.hpp"

#include <algorithm>

#include "game/characters/Character.hpp"
#include "game/objects/Clue.hpp"

void EntityGroup::init()
{
    for (auto& c : characters) {
        c->init();
    }
}

void EntityGroup::addCharacter(std::shared_ptr<Character> c)
{
    characters.push_back(std::move(c));
}

void EntityGroup::addClue(std::unique_ptr<Clue> c)
{
    clues.push_back(std::move(c));
}

const std::vector<std::shared_ptr<Character>>& EntityGroup::getCharacters() const
{
    return characters;
}

const std::vector<std::unique_ptr<Clue>>& EntityGroup::getClues() const
{
    return clues;
}

std::shared_ptr<Character> EntityGroup::getCharacter(int playerNumber) const
{
    for (const auto& c : characters) {
        if (c->player_number == playerNumber) {
            return c;
        }
    }
    return nullptr;
}

Clue* EntityGroup::getClue(int clueNumber) const
{
    for (const auto& c : clues) {
        if (c->clue_number == clueNumber) {
            return c.get();
        }
    }
    return nullptr;
}

void EntityGroup::drawInArea(sf::RenderTarget& ctx, sf::FloatRect box) const
{
    for (const auto& c : characters) {
        if (c->hbox.intersects(box)) {
            ctx.draw(*c);
        }
    }
}

void EntityGroup::onUpdate(float dt)
{
    for (const auto& c : characters) {
        c->update(dt);
    }
    // Sort by z-index so entities further into the scene draw behind closer ones.
    std::sort(characters.begin(), characters.end(),
        [](const std::shared_ptr<GameObject>& a, const std::shared_ptr<GameObject>& b) {
            return a->z_index < b->z_index;
        });
}

void EntityGroup::onDraw(sf::RenderTarget& ctx, sf::RenderStates /*states*/) const
{
    for (const auto& c : characters) {
        ctx.draw(*c);
    }
    for (const auto& c : clues) {
        ctx.draw(*c);
    }
}