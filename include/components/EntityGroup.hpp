#ifndef ENTITYGROUP_HPP
#define ENTITYGROUP_HPP

#include <memory>
#include <vector>
#include <SFML/Graphics.hpp>
#include "engine/GameObject.hpp"

class Character;
class Clue;

/**
 * Owns and renders the interactive entities (characters and clues) for a
 * single play session. Provides lookup by id, ordered drawing by z-index,
 * and a culling helper for drawing only entities within a given area.
 *
 * Ownership note: characters and clues live in the typed vectors below,
 * NOT under GameObject::children. The vectors give us O(1) lookup by
 * player id / clue id and let drawInArea iterate in z-order without
 * walking the generic scene graph. EntityGroup is still a GameObject so
 * it inherits a transform and participates in the screen's draw tree --
 * but its contents are owned here, deliberately.
 */
class EntityGroup : public GameObject
{
public:
    void init() override;

    void addCharacter(std::shared_ptr<Character> c);
    // Clues are owned solely by the EntityGroup; unique_ptr makes that
    // explicit and lets the rest of the game observe with raw pointers.
    void addClue(std::unique_ptr<Clue> clue);

    const std::vector<std::shared_ptr<Character>>& getCharacters() const;
    const std::vector<std::unique_ptr<Clue>>& getClues() const;

    std::shared_ptr<Character> getCharacter(int playerNumber) const;
    Clue* getClue(int clueNumber) const;

    void onUpdate(float dt) override;
    void drawInArea(sf::RenderTarget& ctx, sf::FloatRect box) const;

protected:
    std::vector<std::shared_ptr<Character>> characters;
    std::vector<std::unique_ptr<Clue>> clues;

    void onDraw(sf::RenderTarget& ctx, sf::RenderStates states) const override;
};

#endif
