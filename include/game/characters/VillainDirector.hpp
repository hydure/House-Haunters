#ifndef VILLAIN_DIRECTOR_HPP
#define VILLAIN_DIRECTOR_HPP
////////////////////////////////////////////////////////////
// VillainDirector
//
// The "all-knowing" half of the Alien: Isolation-style ghost AI split.
//
// The Director sits outside the Villain and has perfect, immediate
// information about every Character on the map. It NEVER chases anyone
// itself -- its only job is to answer "who is closest to this point
// right now?" when the Villain pings it.
//
// The Villain ("hunter") deliberately knows nothing about player
// positions on its own. Once per ping interval -- which shrinks as the
// difficulty rises -- it calls into the Director, caches the answer
// as a hint location, and walks toward it. If a character happens to
// step into the Villain's current room the Villain switches to direct
// chase mode and stays locked on that character until they leave the
// room (or die / become invul / become invisible-by-SIS-standing-still).
//
// Eligibility rules (apply to every Director query):
//   * The Villain itself is never returned.
//   * Dead characters (health <= 0) are skipped.
//   * Invulnerable characters are skipped.
//   * A sprinting Brother deliberately takes priority as a noisy decoy.
//   * The Sister (SIS) is invisible to the Director while she is
//     standing still (direction == (0,0)) -- stealth mechanic.
//
// Test hook surface is intentionally tiny so the Director can be
// exercised without instantiating any SFML window, audio device, or
// the rest of the Villain pipeline.
////////////////////////////////////////////////////////////
#include <SFML/System/Vector2.hpp>

class EntityGroup;
class Character;

class VillainDirector
{
public:
    void setEntities(EntityGroup* g) { entities_ = g; }
    EntityGroup* entities() const { return entities_; }

    // Returns the eligible character closest to `origin` by 2D
    // squared distance, or nullptr if no eligible target exists.
    // See the eligibility rules in the file header.
    Character* nearestTo(sf::Vector2f origin) const;

private:
    EntityGroup* entities_ = nullptr;
};

#endif
