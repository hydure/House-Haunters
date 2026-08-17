#include "game/characters/VillainDirector.hpp"
#include "components/EntityGroup.hpp"
#include "game/characters/Character.hpp"
#include "game/Config.hpp"

#include <limits>

Character* VillainDirector::nearestTo(sf::Vector2f origin) const
{
    if (entities_ == nullptr) return nullptr;

    // First pass: figure out whether any non-SIS player is still in
    // play. SIS's "standing-still = invisible" stealth ONLY hides her
    // while at least one teammate is alive to draw the ghost's
    // attention. The moment she's the last living family member, her
    // stealth lapses (otherwise the Director would have no target at
    // all and the ghost would idle forever). She gets a different
    // power in exchange -- see Character::onUpdate's "Final Girl"
    // adrenaline boost.
    bool sisStealthAvailable = false;
    for (const auto& c : entities_->getCharacters()) {
        if (c == nullptr)        continue;
        if (c->isVillain())      continue;
        if (c->health <= 0)      continue;
        if (c->invul)            continue;
        if (c->character == Config::CHARACTER::SIS) continue;
        sisStealthAvailable = true;
        break;
    }

    Character* best  = nullptr;
    float bestDistSq = std::numeric_limits<float>::max();
    bool bestIsNoisy = false;
    for (const auto& c : entities_->getCharacters()) {
        if (c == nullptr)        continue;
        if (c->isVillain())      continue;
        if (c->health <= 0)      continue;
        if (c->invul)            continue;
        // SIS standing still is invisible -- but only while a teammate
        // is alive to attract the ghost instead.
        if (sisStealthAvailable
            && c->character == Config::CHARACTER::SIS
            && c->direction.x == 0 && c->direction.y == 0) {
            continue;
        }
        const sf::Vector2f p = c->getPosition();
        const float dx = p.x - origin.x;
        const float dy = p.y - origin.y;
        const float d2 = dx * dx + dy * dy;
        const bool noisy = c->isDrawingAggro();
        if ((noisy && !bestIsNoisy) || (noisy == bestIsNoisy && d2 < bestDistSq)) {
            bestDistSq = d2;
            best       = c.get();
            bestIsNoisy = noisy;
        }
    }
    return best;
}
