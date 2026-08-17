#include "game/characters/Villain.hpp"
#include "engine/RandomUtil.hpp"
#include "engine/Paths.hpp"
#include "engine/ModConfig.hpp"

#include <cmath>
#include <limits>

namespace {
// Convert ModConfig's flat frame index list (e.g. {1,2,1,0}) into the
// addFrames() shape SpriteAnimation expects ({{1},{2},{1},{0}}). One
// inner vector per frame because SpriteAnimation treats each as a layer
// stack and we only ever paint a single tile per frame.
std::vector<std::vector<int>> toFrameLists(const std::vector<int>& flat)
{
    std::vector<std::vector<int>> out;
    out.reserve(flat.size());
    for (int idx : flat) out.push_back({idx});
    return out;
}
} // namespace

void Villain::init()
{
    this->direction = sf::Vector2f(0, 0);
    // Spawn the villain centered in the first room of the house.
    this->setPosition(
        g->rooms.front()->getPosition().x + ((512 / 2) - 16),
        g->rooms.front()->getPosition().y + ((384 / 2) - 24));
    this->setDirection();

    // Sprite sheet, frame size, walk indices, and starting HP are all
    // data-driven via ModConfig (resources/mods.xml). Vanilla defaults
    // reproduce the original hardcoded values bit-for-bit.
    const auto& vm = ModConfig::instance().villain();
    sprite_map = ResourceManager::getTexture(Paths::resource(vm.sprite_sheet_path));

    walk_down.setSpriteSheet(*sprite_map);
    walk_down.addFrames(toFrameLists(vm.walk_down), vm.frame_width, vm.frame_height);

    walk_left.setSpriteSheet(*sprite_map);
    walk_left.addFrames(toFrameLists(vm.walk_left), vm.frame_width, vm.frame_height);

    walk_right.setSpriteSheet(*sprite_map);
    walk_right.addFrames(toFrameLists(vm.walk_right), vm.frame_width, vm.frame_height);

    walk_up.setSpriteSheet(*sprite_map);
    walk_up.addFrames(toFrameLists(vm.walk_up), vm.frame_width, vm.frame_height);

    curr->stop();

    death_map = ResourceManager::getTexture(Paths::resource("sprites/grave.png"));
    death_animation.setSpriteSheet(*death_map);
    death_animation.addFrames({ {0} }, 32, 32);

    hbox = Hitbox(0, 16, 32, 16);
    hbox.follow(this);
    hbox.init();

    isChasing = false;
    needsCentering = false;
    fastSpeed = false;
    health = vm.health;

    // ---- Two-tier AI setup. ----
    // Hook the Director up to the same EntityGroup we already follow,
    // then prime the ping timer so the very first onUpdate() tick
    // fires a fresh ping (giving the ghost an initial wander hint
    // instead of pingIntervalSec_ seconds of blind wandering).
    director_.setEntities(entity_group);
    hauntElapsedSec_ = 0.f;
    hauntLevel_ = 0;
    pingIntervalSec_ = basePingIntervalSec_;
    pingTimerSec_   = pingIntervalSec_;
    hasDirectorHint_ = false;
    lockedCharacter_ = nullptr;

    enterWanderMode();
}

double Villain::wanderSpeed() { return 90.0; }
double Villain::chaseSpeed()  { return 100.0; }

float Villain::pingIntervalFor(Config::DIFFICULTY d)
{
    // Pings get rarer as difficulty drops: an EASY ghost is acting on
    // ~8-second-old information half the time, while a HARD ghost
    // re-targets every 1.5s and almost always knows where you are.
    switch (d) {
        case Config::EASY:   return 8.0f;
        case Config::NORMAL: return 4.0f;
        case Config::HARD:   return 1.5f;
    }
    return 4.0f;
}

int Villain::hauntLevelFor(float elapsedSeconds)
{
    if (elapsedSeconds >= 180.f) return 2;
    if (elapsedSeconds >= 90.f) return 1;
    return 0;
}

double Villain::pressureMultiplierFor(int hauntLevel)
{
    if (hauntLevel >= 2) return 1.12;
    if (hauntLevel == 1) return 1.06;
    return 1.0;
}

void Villain::setDifficulty(Config::DIFFICULTY d)
{
    basePingIntervalSec_ = pingIntervalFor(d);
    pingIntervalSec_ = basePingIntervalSec_;
}

void Villain::advanceHauntTimer(float dt)
{
    if (dt > 0.f) {
        hauntElapsedSec_ += dt;
    }
    hauntLevel_ = hauntLevelFor(hauntElapsedSec_);
    pingIntervalSec_ = basePingIntervalSec_ * (1.f - 0.2f * hauntLevel_);

    const double multiplier = pressureMultiplierFor(hauntLevel_);
    speed = fastSpeed
        ? chaseSpeed() * multiplier
        : wanderSpeed() * (1.0 + 0.03 * hauntLevel_);
}

void Villain::enterWanderMode()
{
    speed     = wanderSpeed() * (1.0 + 0.03 * hauntLevel_);
    fastSpeed = false;
}

void Villain::enterChaseMode()
{
    speed     = chaseSpeed() * pressureMultiplierFor(hauntLevel_);
    fastSpeed = true;
}

void Villain::pingDirector()
{
    pingTimerSec_ = 0.f;
    Character* target = director_.nearestTo(this->getPosition());
    if (target != nullptr) {
        directorHintPos_ = target->getPosition();
        hasDirectorHint_ = true;
    }
    else {
        hasDirectorHint_ = false;
    }
}

void Villain::setItemDamage(int damage)
{
    healthCut = damage;
}

void Villain::onUpdate(float dt)
{
    // Tick the Director-ping timer. When it elapses, the ghost asks
    // the all-knowing AI "who's closest?" and caches the answer as a
    // wander hint. The Villain itself stays deliberately oblivious
    // between pings -- the staleness is the gameplay knob.
    advanceHauntTimer(dt);
    pingTimerSec_ += dt;
    if (pingTimerSec_ >= pingIntervalSec_) {
        this->pingDirector();
    }

    roomHbox = g->getRoom(this->hbox);
    int dx = static_cast<int>(this->direction.x * speed * dt);
    int dy = static_cast<int>(this->direction.y * speed * dt);

    // If no targetable character is in our room, drift back to the center
    // (when we've been chasing) or wander between rooms.
    if (!this->checkCharacters()) {
        if (isChasing
            && (this->getPosition().x != roomHbox.left + (512 / 2) - 16
             || this->getPosition().y != (roomHbox.top + (384 / 2) - 24))) {
            this->returnToCenter();
        }
        else {
            this->wander();
        }
    }
    else {
        if (!fastSpeed) {
            this->enterChaseMode();
        }
        isChasing = true;
        this->chase();
    }

    this->z_index = static_cast<int>(this->getPosition().y) + 20;

    this->checkCollisions();
    if (health <= 0) {
        curr = &death_animation;
    }

    started = true;
    if (health > 0) {
        this->move(static_cast<float>(dx), static_cast<float>(dy));
    }

    // If we ended up outside any room, teleport to a random non-door room.
    if (!g->isInsideRoom(sf::FloatRect(hbox.left + static_cast<float>(dx),
                                       hbox.top  + static_cast<float>(dy),
                                       hbox.width, hbox.height))) {
        this->randint = randomInt(static_cast<int>(this->g->rooms.size()));
        int count = 0;
        for (auto rmit = g->rooms.begin(); rmit != g->rooms.end(); ++rmit) {
            if (count == randint) {
                if ((*rmit)->hbox == g->getRoom(this->hbox) || (*rmit)->isDoor) {
                    this->randint = randomInt(static_cast<int>(this->g->rooms.size()));
                    rmit = g->rooms.begin();
                    count = 0;
                }
                else {
                    if (fastSpeed) {
                        this->enterWanderMode();
                    }
                    this->setPosition(
                        (*rmit)->hbox.left + (448 / 2) - 16,
                        (*rmit)->hbox.top  + (288 / 2) - 24);
                    this->possiblerooms.clear();
                    this->setDirection();
                    break;
                }
            }
            count++;
        }
    }

    if (dx == 0 && dy == 0) {
        curr->stop();
    }
    else {
        curr->play();
    }
    hbox.onUpdate(dt);
    curr->nextFrame(dt);
}

void Villain::onDraw(sf::RenderTarget& target, sf::RenderStates states) const
{
    target.draw(*curr, states);
    target.draw(hbox);
}

namespace {
    // True when `c` would be skipped by every eligibility check the
    // ghost cares about (it's the villain, it's dead, it's invul, or
    // it's the Sister standing still and therefore invisible).
    // Takes a non-const reference because Character::isVillain() is
    // not declared const; widening that signature is a base-class
    // change beyond this AI refactor.
    bool isUnchaseable(Character& c) {
        if (c.isVillain())                                 return true;
        if (c.health <= 0)                                 return true;
        if (c.invul)                                       return true;
        if (c.character == Config::CHARACTER::SIS
            && c.direction.x == 0 && c.direction.y == 0)   return true;
        return false;
    }
}

bool Villain::checkCharacters()
{
    roomHbox = g->getRoom(this->hbox);

    // ---- Step 1: honor an existing target lock if it still applies.
    //
    // Alien: Isolation parity: once the ghost has spotted a character
    // in its room, it stays locked on that specific character until
    // they leave the room (or die / become invul / become invisible).
    // It does NOT re-evaluate "is THIS character still the closest one
    // in the room?" every tick, which would let a second character
    // walking through steal the chase and let the first slip out.
    if (lockedCharacter_ != nullptr) {
        if (isUnchaseable(*lockedCharacter_)
            || !lockedCharacter_->hbox.intersects(roomHbox)) {
            lockedCharacter_ = nullptr;
        }
        else {
            this->hbox.setColor(sf::Color::Green);
            this->chaseHbox    = lockedCharacter_->hbox;
            this->hasChaseHint = false;
            return true;
        }
    }

    // ---- Step 2: no active lock -- scan the room for a new target.
    for (const auto& c : entity_group->getCharacters()) {
        if (c == nullptr || c.get() == this) continue;
        if (isUnchaseable(*c))               continue;
        if (c->hbox.intersects(roomHbox)) {
            lockedCharacter_   = c.get();
            this->hbox.setColor(sf::Color::Green);
            this->chaseHbox    = c->hbox;
            this->hasChaseHint = false;
            return true;
        }
    }

    // ---- Step 3: nobody to chase. Bridge the cached Director hint
    // (set by the most recent pingDirector() call) over to the wander
    // bias field that setDirection() already understands.
    if (hasDirectorHint_) {
        hasChaseHint    = true;
        chaseHintTarget = directorHintPos_;
    }
    else {
        hasChaseHint = false;
    }
    return false;
}

Character* Villain::nearestTargetCharacter() const
{
    // Backward-compatible single-shot query. The AI itself reaches the
    // Director through the ping cadence (see pingDirector() + onUpdate);
    // this method is kept because the older test_villain_chase.cpp
    // suite and any future ad-hoc callers may still want a synchronous
    // "who is closest right now?" answer.
    return director_.nearestTo(this->getPosition());
}

void Villain::hurt()
{
    health -= healthCut;
    this->randint = randomInt(static_cast<int>(this->g->rooms.size()));
    int count = 0;
    this->direction.x = 0;
    this->direction.y = 0;
    // Lazy, but just go to game over screen.
    if (health <= 0) {
        auto event = std::make_shared< Event<std::string> >("GameEnd");
        Events::triggerEvent("change_screen", event);
    }

    for (auto rmit = g->rooms.begin(); rmit != g->rooms.end(); ++rmit) {
        if (count == randint) {
            if ((*rmit)->hbox == g->getRoom(this->hbox) || (*rmit)->isDoor) {
                this->randint = randomInt(static_cast<int>(this->g->rooms.size()));
                rmit = g->rooms.begin();
                count = 0;
            }
            else {
                if (fastSpeed) {
                    this->enterWanderMode();
                }
                if (health > 0) {
                    this->setPosition(
                        (*rmit)->hbox.left + (448 / 2) - 16,
                        (*rmit)->hbox.top  + (288 / 2) - 36);
                    this->possiblerooms.clear();
                    this->setDirection();
                    break;
                }
            }
        }
        count++;
    }
}

void Villain::returnToCenter()
{
    if (fastSpeed) {
        this->enterWanderMode();
    }
    int xloc = static_cast<int>(this->getPosition().x);
    int yloc = static_cast<int>(this->getPosition().y);
    roomHbox = g->getRoom(this->hbox);

    if (xloc < roomHbox.left + (448 / 2) - 16) {
        this->direction.x = 1;
        curr = &walk_right;
    }
    else if (xloc > roomHbox.left + (448 / 2) - 16) {
        this->direction.x = -1;
        curr = &walk_left;
    }
    else {
        this->direction.x = 0;
    }
    if (yloc > roomHbox.top + (288 / 2) - 36) {
        this->direction.y = -1;
        curr = &walk_up;
    }
    else if (yloc < roomHbox.top + (288 / 2) - 36) {
        this->direction.y = 1;
        curr = &walk_down;
    }
    else {
        this->direction.y = 0;
    }
    // Snap to the exact center if we've overshot by a single pixel.
    if (this->getPosition().x == roomHbox.left + (448 / 2) - 16 - 1) {
        this->setPosition(this->getPosition().x + 1, this->getPosition().y);
        this->direction.x = 0;
    }
    if (this->getPosition().x == roomHbox.left + (448 / 2) - 16 + 1) {
        this->setPosition(this->getPosition().x - 1, this->getPosition().y);
        this->direction.x = 0;
    }
    if (this->getPosition().y == roomHbox.top + (288 / 2) - 36 - 1) {
        this->setPosition(this->getPosition().x, this->getPosition().y + 1);
        this->direction.y = 0;
    }
    if (this->getPosition().y == roomHbox.top + (288 / 2) - 36 + 1) {
        this->setPosition(this->getPosition().x, this->getPosition().y - 1);
        this->direction.y = 0;
    }
    if (this->getPosition().y == roomHbox.top  + (288 / 2) - 36
     && this->getPosition().x == roomHbox.left + (448 / 2) - 16) {
        this->setDirection();
        needsCentering = false;
        isChasing = false;
    }
}

void Villain::chase()
{
    needsCentering = true;

    if (this->chaseHbox.left < this->hbox.left) {
        this->direction.x = -1;
        curr = &walk_left;
    }
    if (this->chaseHbox.left - 10 > this->hbox.left) {
        this->direction.x = 1;
        curr = &walk_right;
    }
    if (this->chaseHbox.top - 10 > this->hbox.top) {
        this->direction.y = 1;
        curr = &walk_down;
    }
    if (this->chaseHbox.top < this->hbox.top) {
        this->direction.y = -1;
        curr = &walk_up;
    }

    for (const auto& c : entity_group->getCharacters()) {
        if (c.get() == this) continue;
        if (this->hbox.intersects(c->hbox) && !c->invul && c->health > 0 && this->health > 0) {
            c->hurt();
            this->randint = randomInt(static_cast<int>(this->g->rooms.size()));
            int count = 0;
            for (auto rmit = g->rooms.begin(); rmit != g->rooms.end(); ++rmit) {
                if (count == randint) {
                    if ((*rmit)->hbox == g->getRoom(this->hbox)) {
                        this->randint = randomInt(static_cast<int>(this->g->rooms.size()));
                    }
                    else {
                        if (fastSpeed) {
                            this->enterWanderMode();
                        }
                        this->setPosition(
                            (*rmit)->hbox.left + (448 / 2) - 16,
                            (*rmit)->hbox.top  + (288 / 2) - 36);
                        this->possiblerooms.clear();
                        this->setDirection();
                        // After a successful hit-and-teleport the
                        // ghost has "escaped" -- drop the lock so the
                        // next room scan starts cleanly.
                        lockedCharacter_ = nullptr;
                        break;
                    }
                }
                count++;
            }
        }
    }
}

void Villain::wander()
{
    int xloc = static_cast<int>(this->getPosition().x);
    int yloc = static_cast<int>(this->getPosition().y);
    if (needsCentering) {
        if (this->getPosition().x == roomHbox.left + (448 / 2) - 16 - 1) {
            this->setPosition(this->getPosition().x + 1, this->getPosition().y);
            needsCentering = false;
            this->setDirection();
        }
        if (this->getPosition().x == roomHbox.left + (448 / 2) - 16 + 1) {
            this->setPosition(this->getPosition().x - 1, this->getPosition().y);
            needsCentering = false;
            this->setDirection();
        }
        if (this->getPosition().y == roomHbox.top + (288 / 2) - 36 - 1) {
            this->setPosition(this->getPosition().x, this->getPosition().y + 1);
            needsCentering = false;
            this->setDirection();
        }
        if (this->getPosition().y == roomHbox.top + (288 / 2) - 36 + 1) {
            this->setPosition(this->getPosition().x, this->getPosition().y - 1);
            needsCentering = false;
            this->setDirection();
        }
    }
    else {
        if (xloc == roomHbox.left + (448 / 2) - 16
         && yloc == roomHbox.top  + (288 / 2) - 36) {
            this->possiblerooms.clear();
            this->setDirection();
        }
        else {
            if (this->getPosition().x == roomHbox.left + (448 / 2) - 16 - 1
             || this->getPosition().x == roomHbox.left + (448 / 2) - 16 + 1
             || this->getPosition().y == roomHbox.top  + (384 / 2) - 24 - 1
             || this->getPosition().y == roomHbox.top  + (384 / 2) - 24 + 1) {
                if (this->direction.x != 0
                 && this->getPosition().y != roomHbox.top + (288 / 2) - 36) {
                    this->setPosition(this->getPosition().x, roomHbox.top + (288 / 2) - 36);
                }
                if (this->direction.y != 0
                 && this->getPosition().x != roomHbox.left + (448 / 2) - 16) {
                    this->setPosition(roomHbox.left + (448 / 2) - 16, this->getPosition().y);
                    this->setDirection();
                }
            }
        }
    }
}

void Villain::setDirection()
{
    // Look for adjacent rooms in each cardinal direction, avoiding immediate backtracking.
    if (g->isInsideRoom(sf::FloatRect(this->getPosition().x + 512, this->getPosition().y, hbox.width, hbox.height))
        && previousString != "right") {
        this->possiblerooms.push_back("right");
    }
    if (g->isInsideRoom(sf::FloatRect(this->getPosition().x - 512, this->getPosition().y, hbox.width, hbox.height))
        && previousString != "left") {
        this->possiblerooms.push_back("left");
    }
    if (g->isInsideRoom(sf::FloatRect(this->getPosition().x, this->getPosition().y + 384, hbox.width, hbox.height))
        && previousString != "down") {
        this->possiblerooms.push_back("down");
    }
    if (g->isInsideRoom(sf::FloatRect(this->getPosition().x, this->getPosition().y - 384, hbox.width, hbox.height))
        && previousString != "up") {
        this->possiblerooms.push_back("up");
    }
    // If nothing turned up, keep going the way we came.
    if (this->possiblerooms.empty()) {
        if      (this->previousString == "left")  this->possiblerooms.push_back("left");
        else if (this->previousString == "right") this->possiblerooms.push_back("right");
        else if (this->previousString == "up")    this->possiblerooms.push_back("up");
        else if (this->previousString == "down")  this->possiblerooms.push_back("down");
    }

    // Gravitate toward the closest target across the whole map. When
    // checkCharacters() couldn't find a same-room target it stashed the
    // closest player's position into chaseHintTarget; here we pick the
    // adjacent room whose direction most reduces the distance to that
    // hint. Falls back to random selection when no hint exists, or when
    // none of the candidate rooms makes progress toward the target.
    std::string next;
    if (hasChaseHint && !this->possiblerooms.empty()) {
        const float dx = chaseHintTarget.x - this->getPosition().x;
        const float dy = chaseHintTarget.y - this->getPosition().y;
        std::string preferredX, preferredY;
        if (std::abs(dx) >= 1.f) preferredX = (dx > 0.f) ? "right" : "left";
        if (std::abs(dy) >= 1.f) preferredY = (dy > 0.f) ? "down"  : "up";
        // Prefer the major-axis direction first; fall through to the
        // minor axis when the major one is unavailable.
        const std::string major = (std::abs(dx) >= std::abs(dy)) ? preferredX : preferredY;
        const std::string minor = (std::abs(dx) >= std::abs(dy)) ? preferredY : preferredX;
        for (const auto& r : this->possiblerooms) {
            if (!major.empty() && r == major) { next = r; break; }
        }
        if (next.empty()) {
            for (const auto& r : this->possiblerooms) {
                if (!minor.empty() && r == minor) { next = r; break; }
            }
        }
    }
    if (next.empty()) {
        this->randint = randomInt(static_cast<int>(this->possiblerooms.size()));
        next = this->possiblerooms.at(randint);
    }
    if (next == "right") {
        this->previousString = "left";
        this->direction.x = 1;
        this->direction.y = 0;
        curr = &walk_right;
    }
    if (next == "left") {
        this->previousString = "right";
        this->direction.x = -1;
        this->direction.y = 0;
        curr = &walk_left;
    }
    if (next == "up") {
        this->previousString = "down";
        this->direction.y = -1;
        this->direction.x = 0;
        curr = &walk_up;
    }
    if (next == "down") {
        this->previousString = "up";
        this->direction.y = 1;
        this->direction.x = 0;
        curr = &walk_down;
    }
}
