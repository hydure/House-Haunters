#include <algorithm>
#include <unordered_map>
#include "game/characters/Character.hpp"
#include "game/InvestigationJournal.hpp"
#include "game/characters/Villain.hpp"
#include "game/objects/Clue.hpp"
#include "engine/RandomUtil.hpp"
#include "engine/Paths.hpp"
#include "engine/ModConfig.hpp"
namespace {
// Returns the y-offset (relative to the room's top) at which a player
// should spawn for the given room layout. Defaults to 160 for unknown rooms.
int spawnYOffsetFor(const std::string& room_setup)
{
    static const std::unordered_map<std::string, int> kSpawnY = {
        {"grave",        128},
        {"parlor",       128},
        {"wood_bedroom", 128},
        {"bathroom",      96},
        {"dungeon",      256},
    };
    auto it = kSpawnY.find(room_setup);
    return it == kSpawnY.end() ? 160 : it->second;
}
} // namespace

void Character::init()
{
    chara_hurt.load(Paths::resource(ModConfig::instance().audio().player_hurt_sfx));
    chara_death.load(Paths::resource(ModConfig::instance().audio().player_death_sfx));
    ghost_sound.load(Paths::resource(ModConfig::instance().audio().ghost_chase_sfx));
    clue_found_sound.load(Paths::resource(ModConfig::instance().audio().clue_found_sfx));
    weapon_select_sound.load(Paths::resource(ModConfig::instance().audio().weapon_select_sfx));

    this->direction = sf::Vector2f(0, 0);
    this->setOrigin(16, 16);

    // Per-character stat / sprite data comes from ModConfig (which reads
    // resources/mods.xml at startup, or returns vanilla defaults when no
    // mod file is present). Behavioral specials -- MOM's clue upgrade,
    // BRO's sprint toggle, SIS's stationary stealth, DAD's jackpot damage
    // -- stay keyed off Character::character in their respective methods.
    const auto& cmod = ModConfig::instance().character(static_cast<int>(character));
    health    = cmod.health;
    maxHealth = cmod.max_health;
    speed *= cmod.speed_multiplier;
    int sprite_location = cmod.sprite_location;
    hasItem = false;
    itemDamage = 1;

    // Spawn somewhere inside a random non-door room.
    int random_room = randomInt(g->roomCount());
    Room* room = g->getRoom(random_room);
    int yOffset = spawnYOffsetFor(room->room_setup);
    this->setPosition(
        room->hbox.left + 20 + (32 * player_number),
        room->hbox.top  + yOffset);

    sprite_map = ResourceManager::getTexture(Paths::resource(cmod.sprite_sheet_path));

    // Each character's row in the spritesheet is offset by `mod` frames.
    int x = sprite_location % 2;
    int y = sprite_location / 2;
    int mod = 3 * x + 24 * y;

    attack_anim.setSpriteSheet(*ResourceManager::getTexture(Paths::resource("sprites/swipe.png")));
    attack_anim.setPosition(16, 0);
    attack_anim.addFrames({ {0}, {1}, {2}, {3} }, 32, 32);

    walk_down.setSpriteSheet(*sprite_map);
    walk_down.addFrames({ {mod + 1}, {mod + 2}, {mod + 1}, {mod + 0} }, 32, 32);

    walk_left.setSpriteSheet(*sprite_map);
    walk_left.addFrames({ {7 + mod}, {8 + mod}, {7 + mod}, {6 + mod} }, 32, 32);

    walk_right.setSpriteSheet(*sprite_map);
    walk_right.addFrames({ {13 + mod}, {14 + mod}, {13 + mod}, {12 + mod} }, 32, 32);

    walk_up.setSpriteSheet(*sprite_map);
    walk_up.addFrames({ {19 + mod}, {20 + mod}, {19 + mod}, {18 + mod} }, 32, 32);

    death_map = ResourceManager::getTexture(Paths::resource("sprites/grave.png"));
    death_animation.setSpriteSheet(*death_map);
    death_animation.addFrames({ {0} }, 32, 32);

    curr = &walk_down;
    curr->stop();

    hbox = Hitbox(-8, 0, 16, 16);
    hbox.follow(this);
    hbox.init();
    invul = false;
}

void Character::setItemDamage(int damaging)
{
    itemDamage = damaging;
}

bool Character::isLastAlive() const
{
    // Defensive: in unit tests the entity group may not be wired up
    // yet; treat that as "I can't prove anyone else is alive". We
    // still need to be alive ourselves -- a dead character is not
    // the last alive, the team has lost.
    if (entity_group == nullptr) return false;
    if (this->health <= 0)       return false;
    for (const auto& c : entity_group->getCharacters()) {
        if (c == nullptr || c.get() == this) continue;
        if (c->isVillain())                  continue;
        if (c->health <= 0)                  continue;
        return false;
    }
    return true;
}

bool Character::hasLivingTeammateInRoom() const
{
    if (entity_group == nullptr || currentRoom == nullptr) return false;
    for (const auto& teammate : entity_group->getCharacters()) {
        if (!teammate || teammate.get() == this || teammate->isVillain()) continue;
        if (teammate->health > 0 && teammate->currentRoom == currentRoom) return true;
    }
    return false;
}

bool Character::redirectHitToDad()
{
    if (entity_group == nullptr || currentRoom == nullptr
        || character == Config::CHARACTER::DAD) {
        return false;
    }
    for (const auto& teammate : entity_group->getCharacters()) {
        if (!teammate || teammate.get() == this || teammate->isVillain()) continue;
        if (teammate->character != Config::CHARACTER::DAD) continue;
        if (teammate->health <= 0 || teammate->invul) continue;
        if (teammate->currentRoom != currentRoom) continue;
        if (teammate->dadProtectionCooldownSec_ > 0.f) continue;

        teammate->health--;
        teammate->invul = true;
        teammate->dadProtectionCooldownSec_ = 10.f;
        return true;
    }
    return false;
}

bool Character::anyChildHasDied() const
{
    if (entity_group == nullptr) return false;
    for (const auto& c : entity_group->getCharacters()) {
        if (c == nullptr || c.get() == this) continue;
        if (c->isVillain())                  continue;
        const auto k = c->character;
        if (k != Config::CHARACTER::BRO && k != Config::CHARACTER::SIS) continue;
        if (c->health <= 0) return true;
    }
    return false;
}

bool Character::hasFatherDied() const
{
    if (entity_group == nullptr) return false;
    for (const auto& c : entity_group->getCharacters()) {
        if (c == nullptr || c.get() == this) continue;
        if (c->isVillain())                  continue;
        if (c->character != Config::CHARACTER::DAD) continue;
        if (c->health <= 0) return true;
    }
    return false;
}

bool Character::hasMotherDied() const
{
    if (entity_group == nullptr) return false;
    for (const auto& c : entity_group->getCharacters()) {
        if (c == nullptr || c.get() == this) continue;
        if (c->isVillain())                  continue;
        if (c->character != Config::CHARACTER::MOM) continue;
        if (c->health <= 0) return true;
    }
    return false;
}

int Character::deadChildCount() const
{
    if (entity_group == nullptr) return 0;
    int n = 0;
    for (const auto& c : entity_group->getCharacters()) {
        if (c == nullptr || c.get() == this) continue;
        if (c->isVillain())                  continue;
        const auto k = c->character;
        if (k != Config::CHARACTER::BRO && k != Config::CHARACTER::SIS) continue;
        if (c->health <= 0) ++n;
    }
    return n;
}

void Character::applyFinalGirlIfDue()
{
    // Only SIS gets the Final Girl transition, and only once. The
    // multiplier composes with whatever speed she currently has --
    // including the mod-driven base speed set in init() -- so a mod
    // that retunes SIS's base also retunes her Final Girl speed.
    if (finalGirlActive)                              return;
    if (character != Config::CHARACTER::SIS)          return;
    if (!isLastAlive())                               return;
    speed *= SIS_FINAL_GIRL_MULTIPLIER;
    finalGirlActive = true;
}

void Character::applyMaternalFuryIfDue()
{
    // Only MOM, and only once. Triggers as soon as any child has
    // died. MOM must be alive herself -- a dead MOM can't be furious.
    if (maternalFuryActive)                           return;
    if (character != Config::CHARACTER::MOM)          return;
    if (this->health <= 0)                            return;
    if (!anyChildHasDied())                           return;
    strengthBonus += MOM_MATERNAL_FURY_BONUS;
    maternalFuryActive = true;
}

void Character::applyManOfHouseIfDue()
{
    // Only BRO, and only once. Triggers the moment DAD is dead.
    // BRO must be alive himself -- a dead BRO can't man up.
    if (manOfHouseActive)                             return;
    if (character != Config::CHARACTER::BRO)          return;
    if (this->health <= 0)                            return;
    if (!hasFatherDied())                             return;
    strengthBonus += BRO_MAN_OF_HOUSE_BONUS;
    manOfHouseActive = true;
}

void Character::applyKeepsTakingIfDue()
{
    // DAD's "It Just Keeps Taking and Taking". Only DAD, only
    // once, only if he's still alive -- the doubling is a buff,
    // not a revive. We double his ORIGINAL maxHealth and heal him
    // to the new cap so the ghost has to chew through twice as
    // many hits to put him down. Composes additively with any
    // future maxHealth tweaks because we read the current value
    // when we double, but the flag guarantees we only do it once.
    if (keepsTakingActive)                            return;
    if (character != Config::CHARACTER::DAD)          return;
    if (this->health <= 0)                            return;
    if (!hasMotherDied())                             return;
    maxHealth *= DAD_KEEPS_TAKING_MULTIPLIER;
    health     = maxHealth;
    keepsTakingActive = true;
}

void Character::applyPapaBearIfDue()
{
    // DAD's "Papa Bear". Only DAD, only while he's alive. Unlike
    // every other endgame passive this one is NOT a one-shot --
    // every time a new child dies, DAD gets another +bonus. The
    // per-child amount is half of MOM's Maternal Fury bonus (see
    // DAD_PAPA_BEAR_BONUS in the header): MOM cares whether ANY
    // child has died, DAD grieves progressively. We diff the
    // current dead-child count against the count we've already
    // credited so subsequent ticks don't re-add the same bonus
    // for the same child. There is no revive mechanic so the
    // count is monotonically non-decreasing, which makes the diff
    // safe to clamp at zero.
    if (character != Config::CHARACTER::DAD)          return;
    if (this->health <= 0)                            return;
    const int dead = deadChildCount();
    if (dead <= papaBearCreditedChildren)             return;
    const int delta = dead - papaBearCreditedChildren;
    strengthBonus += delta * DAD_PAPA_BEAR_BONUS;
    papaBearCreditedChildren = dead;
}

void Character::checkVillain()
{
    for (const auto& c : entity_group->getCharacters()) {
        if (!c->isVillain()) continue;
        if (c->hbox == this->hbox) continue;
        // strengthBonus is the shared additive-damage hook for
        // MOM's Maternal Fury, BRO's Man of the House, and DAD's
        // Papa Bear. Zero for every character with no power active,
        // so this is a no-op for them.
        c->setItemDamage(itemDamage + strengthBonus);

        const bool inHorizontalSlice =
            c->hbox.top >= this->hbox.top - 32 && c->hbox.top <= this->hbox.top + 32;
        const bool inVerticalSliceLeft =
            c->hbox.left >= this->hbox.left - 32 && c->hbox.top <= this->hbox.left + 32;
        const bool inVerticalSliceDown =
            c->hbox.left >= this->hbox.left - 20 && c->hbox.top <= this->hbox.left + 36;

        if (curr == &walk_right
            && this->hbox.left + this->hbox.width + 64 > c->hbox.left
            && this->hbox.left + this->hbox.width      < c->hbox.left
            && inHorizontalSlice) {
            c->hurt();
        }
        if (curr == &walk_left
            && this->hbox.left - 64 < c->hbox.left + c->hbox.width
            && this->hbox.left      > c->hbox.left + c->hbox.width
            && inHorizontalSlice) {
            c->hurt();
        }
        if (curr == &walk_up
            && this->hbox.top - 64 < c->hbox.top + c->hbox.height
            && this->hbox.top      > c->hbox.top + c->hbox.height
            && inVerticalSliceLeft) {
            c->hurt();
        }
        if (curr == &walk_down
            && this->hbox.top + this->hbox.height + 64 > c->hbox.top
            && this->hbox.top + this->hbox.height      < c->hbox.top
            && inVerticalSliceDown) {
            c->hurt();
        }
    }
}

void Character::checkClues()
{
    // Only look at clues that physically live in our current room (cached
    // in onUpdate). currentRoom may be nullptr for the very first tick or
    // while the character is in a doorway -- treat that as "no clues to
    // collide with" and clear any stale state.
    if (currentRoom == nullptr) {
        this->stopLeft  = false;
        this->stopRight = false;
        this->stopUp    = false;
        this->stopDown  = false;
        this->currentClue = nullptr;
        atClue = false;
        return;
    }
    for (Clue* c : currentRoom->cluesInRoom) {
        if (c->hbox.intersects(this->hbox)) {
            if (this->hbox.left + this->hbox.width - c->hbox.left >= 0
                && this->hbox.left + this->hbox.width - c->hbox.left <= 4) {
                this->stopRight = true;
            }
            else if (c->hbox.left + c->hbox.width - this->hbox.left >= 0
                  && c->hbox.left + c->hbox.width - this->hbox.left <= 4) {
                this->stopLeft = true;
            }
            else if (c->hbox.top + c->hbox.height - this->hbox.top >= 0
                  && c->hbox.top + c->hbox.height - this->hbox.top <= 4) {
                this->stopUp = true;
            }
            else if (this->hbox.top + this->hbox.height - c->hbox.top >= 0
                  && this->hbox.top + this->hbox.height - c->hbox.top <= 4) {
                this->stopDown = true;
            }
            this->currentClue = c;
            this->hbox.setColor(sf::Color::Green);
            atClue = true;
            return;
        }
    }
    this->stopLeft  = false;
    this->stopRight = false;
    this->stopUp    = false;
    this->stopDown  = false;
    this->currentClue = nullptr;
    atClue = false;
}

void Character::onUpdate(float dt)
{
    if (dadProtectionCooldownSec_ > 0.f) {
        dadProtectionCooldownSec_ = std::max(0.f, dadProtectionCooldownSec_ - dt);
    }
    // SIS's "Final Girl" passive: the moment she becomes the only
    // living family member, lock in the speed boost. Idempotent --
    // there is no revive mechanic, so once she's alone she stays
    // alone until she dies (or wins).
    this->applyFinalGirlIfDue();
    // MOM's "Maternal Fury" passive: the first time one of her
    // children dies, lock in the strength bonus.
    this->applyMaternalFuryIfDue();
    // BRO's "Man of the House" passive: when DAD dies, the son
    // inherits the protector role and gains a strength bonus.
    this->applyManOfHouseIfDue();
    // DAD's "It Just Keeps Taking and Taking" passive: when MOM
    // dies, DAD's maxHealth doubles and he's healed to the new cap.
    this->applyKeepsTakingIfDue();
    // DAD's "Papa Bear" passive: each child's death grows DAD's
    // strength bonus. NOT idempotent across distinct child deaths.
    this->applyPapaBearIfDue();

    float dx = static_cast<float>(this->direction.x * speed * dt);
    float dy = static_cast<float>(this->direction.y * speed * dt);

    // Only move while we're inside a room and still alive.
    if (g->isInsideRoom(sf::FloatRect(hbox.left + dx, hbox.top + dy, hbox.width, hbox.height))
        && health > 0) {
        // Refresh the cached room once per tick. Used by checkClues (this
        // function) and PlayerView (drawing) so neither has to walk the
        // full room list.
        currentRoom = g->getRoomInside(this->hbox);
        this->checkClues();
        if (this->stopLeft  && dx < 0) dx = 0;
        if (this->stopRight && dx > 0) dx = 0;
        if (this->stopUp    && dy < 0) dy = 0;
        if (this->stopDown  && dy > 0) dy = 0;
        this->move(dx, dy);
    }

    if (invul && !isStarted) {
        this->clock.restart();
        isStarted = true;
    }
    if (this->clock.getElapsedTime().asSeconds() >= 3 && this->invul) {
        this->invul = false;
        isStarted = false;
    }
    if (this->isAlive && health <= 0) {
        curr = &death_animation;
        this->isAlive = false;
        auto e = std::make_shared< Event<bool> >("true");
        Events::queueEvent("player_died", e);
    }

    this->z_index = static_cast<int>(this->getPosition().y);

    this->checkCollisions();

    if (dx == 0 && dy == 0) {
        curr->stop();
    }
    else {
        curr->play();
    }
    hbox.onUpdate(dt);
    if (isAttacking) {
        attack_anim.nextFrame(dt);
    }
    curr->nextFrame(dt);
}

void Character::checkCollisions()
{
    this->hbox.setColor(sf::Color::Yellow);
    for (const auto& c : entity_group->getCharacters()) {
        if (c.get() == this) continue;
        if (this->hbox.intersects(c->hbox)) {
            this->hbox.setColor(sf::Color::Red);
        }
    }
}

void Character::hurt()
{
    if (redirectHitToDad()) {
        return;
    }
    this->health--;
    this->invul = true;
    if (health > 0) {
        chara_hurt.play();
        ghost_sound.play();
    }
    else {
        chara_death.play();
    }
}

void Character::attack()
{
    if (!this->isAttacking) {
        this->isAttacking = true;
        attack_anim.play([=]() {
            attack_anim.stop();
            this->isAttacking = false;
        });
        this->checkVillain();
    }
}

void Character::onDraw(sf::RenderTarget& target, sf::RenderStates states) const
{
    if (isAttacking) {
        target.draw(attack_anim, states);
    }
    target.draw(*curr, states);
    target.draw(hbox);
}

void Character::onGamepadEvent(GamepadEvent e)
{
    if (health <= 0) {
        return;
    }
    switch (e.type) {
        case GamepadEvent::TYPE::RELEASED:
            if (e.button == "UP" || e.button == "DOWN") {
                this->direction.y = 0;
            }
            else if (e.button == "LEFT" || e.button == "RIGHT") {
                this->direction.x = 0;
            }
            else if (e.button == "X" && character == Config::CHARACTER::BRO) {
                this->speed /= 2;
                drawingAggro_ = false;
            }
            if (this->direction.y <= -1) curr = &walk_up;
            if (this->direction.y >=  1) curr = &walk_down;
            if (this->direction.x <= -1) curr = &walk_left;
            if (this->direction.x >=  1) curr = &walk_right;
            if (e.button == "A" && readClue && this->currentClue != nullptr) {
                readClue = false;
                this->currentClue->close();
            }
            break;

        case GamepadEvent::TYPE::PRESSED:
            if (e.button == "UP") {
                pressDirection(&walk_up,    -90.f, sf::Vector2f(0,  16), sf::Vector2f(0, -1), stopUp);
            }
            if (e.button == "DOWN") {
                pressDirection(&walk_down,   90.f, sf::Vector2f(32, 16), sf::Vector2f(0,  1), stopDown);
            }
            if (e.button == "LEFT") {
                pressDirection(&walk_left,  180.f, sf::Vector2f(16, 32), sf::Vector2f(-1, 0), stopLeft);
            }
            if (e.button == "RIGHT") {
                pressDirection(&walk_right,   0.f, sf::Vector2f(16,  0), sf::Vector2f(1,  0), stopRight);
            }
            if (e.button == "B") {
                this->attack();
            }
            if (e.button == "X" && character == Config::CHARACTER::BRO) {
                this->speed *= 2;
                drawingAggro_ = true;
            }
            if (e.button == "Y") {
                cycleWeapon();
            }
            if (e.button == "A" && !readClue && this->currentClue != nullptr) {
                openHeldClue();
            }
            break;
    }
}

// Sets attack-swipe orientation, walking sprite, and direction for a held
// directional input. If the character is blocked along that axis (e.g. up
// against a clue), keeps the velocity at zero so they don't slide into it.
void Character::pressDirection(SpriteAnimation* anim,
                               float attackRotation,
                               sf::Vector2f attackPos,
                               sf::Vector2f dir,
                               bool blocked)
{
    curr = anim;
    if (blocked) {
        if (dir.x != 0) this->direction.x = 0;
        if (dir.y != 0) this->direction.y = 0;
        return;
    }
    attack_anim.setRotation(attackRotation);
    attack_anim.setPosition(attackPos);
    if (dir.x != 0) this->direction.x = dir.x;
    if (dir.y != 0) this->direction.y = dir.y;
}

// A-button press while standing on an unread clue: flip into read mode,
// apply MOM's hunch upgrade, open the clue, and add it to the shared case file.
void Character::openHeldClue()
{
    readClue = true;
    // MOM has a 50% chance to upgrade a worthless clue into a vague one
    // before reading it -- her "hunch" ability.
    if (character == Config::CHARACTER::MOM
        && this->currentClue->setClue == this->currentClue->clueWorthless
        && (hasLivingTeammateInRoom() || randomInt(2) == 0)) {
        this->currentClue->setClue = this->currentClue->clueVague;
    }
    this->currentClue->open();

    InvestigationJournal::Tier tier = InvestigationJournal::Tier::WORTHLESS;
    if (this->currentClue->setClue == this->currentClue->clueJackpot) {
        tier = InvestigationJournal::Tier::JACKPOT;
    }
    else if (this->currentClue->setClue == this->currentClue->clueSpec) {
        tier = InvestigationJournal::Tier::SPECIFIC;
    }
    else if (this->currentClue->setClue == this->currentClue->clueVague) {
        tier = InvestigationJournal::Tier::VAGUE;
    }
    if (InvestigationJournal::instance().discover(
            this->currentClue->setClue, tier, player_number)) {
        clue_found_sound.play();
    }
}

void Character::cycleWeapon()
{
    selectedWeapon_ = WeaponSystem::next(selectedWeapon_);
    itemDamage = WeaponSystem::instance().damageFor(selectedWeapon_);
    if (character == Config::CHARACTER::DAD && itemDamage > 1) {
        itemDamage += 2;
    }
    hasItem = true;
    weapon_select_sound.play();
}
