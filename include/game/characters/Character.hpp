#ifndef CHARACTER_HPP
#define CHARACTER_HPP

#include <memory>
#include <SFML/Graphics.hpp>
#include "engine/Engine.hpp"
#include "engine/AudioPlayer.hpp"
#include "components/SpriteAnimation.hpp"
#include "components/Hitbox.hpp"
#include "game/rooms/Room.hpp"
#include "game/rooms/RoomGroup.hpp"
#include "components/EntityGroup.hpp"
////////////////
// Character.hpp
//
// A playable family member. Wanders the house, picks up clues, can attack the
// villain when their swing animation overlaps it.
//
////////////////

class Character : public GameObject
{
public:
    Character() = default;

    void setCharacter(Config::CHARACTER c) { character = c; }
    void setEntities(EntityGroup* entities) { entity_group = entities; }
    /**
    * Stores the room group.
    *   *note*
    *       Having this stored here is dangerous. There's a small possibility that we might
    *       accidentally delete it before the character is done with it.
    *       We might consider storing a reference to the GameplayScreen (which houses the RoomGroup)
    *       and then "asking" politely for the RoomGroup when we need it.
    */
    void setRoomGroup(RoomGroup* group) { g = group; }

    void setPlayerNumber(int number) { player_number = number; }
    void setGamepadIndex(int number) { gamepad_index = number; }
    int  getGamepadIndex() const { return gamepad_index; }

    /** Translates gamepad input into movement, attacks, and clue interactions. */
    virtual void onGamepadEvent(GamepadEvent e);
    /** Very simple collision checking. */
    virtual void checkCollisions();
    /** Override point used by Villain for its own hurt behavior. */
    virtual void hurt();

    /* See GameObject. */
    virtual void init() override;
    virtual void onDraw(sf::RenderTarget& target, sf::RenderStates states) const override;
    virtual void onUpdate(float dt) override;

    void checkClues();
    void checkVillain();
    void attack();
    virtual bool isVillain() { return false; }
    virtual void setItemDamage(int itemDamage);

    // True when no OTHER living non-villain character exists in the
    // shared entity group. Used by SIS's "Final Girl" passive (see
    // SIS_FINAL_GIRL_MULTIPLIER) and exposed publicly so the test
    // harness can drive it without a full game tick.
    bool isLastAlive() const;
    // True when at least one of MOM's children (BRO or SIS) currently
    // sits at health <= 0. Drives MOM's "Maternal Fury" passive.
    bool anyChildHasDied() const;
    // True when DAD currently sits at health <= 0. Drives BRO's
    // "Man of the House" passive.
    bool hasFatherDied() const;
    // True when MOM currently sits at health <= 0. Drives DAD's
    // "It Just Keeps Taking and Taking" passive.
    bool hasMotherDied() const;
    // How many of MOM's children (BRO + SIS) are currently dead.
    // Drives DAD's "Papa Bear" passive, which adds strength every
    // time a NEW child dies (so losing both children stacks).
    int deadChildCount() const;
    // Test hook: apply SIS's Final Girl transition once if the
    // conditions are met. Idempotent. The game runs this from
    // onUpdate; tests call it directly to avoid spinning up sprites.
    void applyFinalGirlIfDue();
    // Test hook: apply MOM's Maternal Fury transition once if the
    // conditions are met. Idempotent. Same call site as Final Girl.
    void applyMaternalFuryIfDue();
    // Test hook: apply BRO's Man of the House transition once if the
    // conditions are met. Idempotent. Same call site as Final Girl.
    void applyManOfHouseIfDue();
    // Test hook: apply DAD's "It Just Keeps Taking and Taking"
    // transition once if MOM is dead. Idempotent.
    void applyKeepsTakingIfDue();
    // Test hook: apply DAD's "Papa Bear" transition. NOT idempotent
    // -- each newly-dead child grants another +bonus. Called every
    // tick; the credited-count guard ensures we only add the delta.
    void applyPapaBearIfDue();
    // Read-only accessor for the post-mod-multiplier movement speed.
    // Tests use this to pin the SIS_FINAL_GIRL_MULTIPLIER invariant.
    double currentSpeed() const { return speed; }
    // Read-only accessor for the additive strength bonus applied to
    // every villain swing this character lands. Tests use this to
    // pin MOM_MATERNAL_FURY_BONUS.
    int currentStrengthBonus() const { return strengthBonus; }
    // Read-only accessor for DAD's current maxHealth. Tests use
    // this to pin DAD_KEEPS_TAKING_MULTIPLIER.
    int currentMaxHealth() const { return maxHealth; }

    int player_number = -1;
    // create a hitbox at bottom half of 32x32 character
    Hitbox hbox;
    int health = 0;
    int maxHealth = 0;
    bool invul = false;
    bool readClue = false;
    bool atClue = false;
    // Non-owning. Clue lifetimes belong to EntityGroup.
    Clue* currentClue = nullptr;
    // Cached "room we're currently standing inside" -- refreshed once per
    // onUpdate. Lets PlayerView and checkClues avoid walking every room
    // every frame. Non-owning (rooms are owned by RoomGroup).
    Room* currentRoom = nullptr;
    sf::Vector2f direction;
    Config::CHARACTER character = Config::CHARACTER::MOM;
    bool hasItem = false;
    int itemDamage = 1;

    // SIS's "Final Girl" power: when she becomes the last living
    // family member, her stealth ability stops working (the Director
    // now targets her even when stationary) but her base movement
    // speed is permanently boosted by this multiplier, giving her a
    // real escape tool when she's alone. Applied exactly once, at
    // the moment she becomes the last alive, to avoid float drift.
    // Mirrors BRO's sprint in spirit but is a passive instead of a
    // held-button. Tests pin both the trigger condition and the
    // resulting speed > Villain::chaseSpeed() invariant.
    static constexpr double SIS_FINAL_GIRL_MULTIPLIER = 1.5;
    bool finalGirlActive = false;

    // MOM's "Maternal Fury" power: the first time one of her children
    // (BRO or SIS) dies, she gains a permanent +bonus to the damage
    // she deals on every villain hit. Stacks additively with her
    // existing weapon damage (so the jackpot-weapon assignment in
    // openHeldClue does not wipe the bonus). Applied exactly once.
    static constexpr int MOM_MATERNAL_FURY_BONUS = 2;
    bool maternalFuryActive = false;
    // BRO's "Man of the House" power: when DAD dies, the son steps
    // up to fill his father's shoes. Gains a permanent +bonus to
    // the damage he deals on every villain hit. Larger than MOM's
    // bonus (he's the bruiser inheriting DAD's protector role), and
    // composes with his sprint and any jackpot weapon. Applied
    // exactly once.
    static constexpr int BRO_MAN_OF_HOUSE_BONUS = 3;
    bool manOfHouseActive = false;
    // DAD's "It Just Keeps Taking and Taking" power: when MOM (his
    // wife) dies, DAD doubles his original maxHealth and is healed
    // to the new cap. He's now a damage sponge -- the ghost has to
    // wail on him twice as long to put him down. Applied exactly
    // once, locked behind keepsTakingActive.
    static constexpr int DAD_KEEPS_TAKING_MULTIPLIER = 2;
    bool keepsTakingActive = false;
    // DAD's "Papa Bear" power: gains +bonus strength every time a
    // NEW child of his dies. Unlike MOM's one-shot Maternal Fury,
    // this stacks per child (so losing both BRO and SIS gives +2 *
    // BONUS). The credited counter tracks how many of the currently
    // dead children DAD has already "avenged" so we don't double-
    // count on subsequent ticks. There is no revive mechanic so
    // the count is monotonically non-decreasing.
    //
    // The per-child bonus is intentionally HALF of MOM's one-shot
    // bonus: she cares whether ANY of her kids has died (full
    // bonus on the first death), while DAD grieves progressively
    // and only matches her bonus once he's lost both children.
    // Derived from MOM_MATERNAL_FURY_BONUS so re-tuning her bonus
    // automatically re-tunes his per-child increment.
    static constexpr int DAD_PAPA_BEAR_BONUS = MOM_MATERNAL_FURY_BONUS / 2;
    int papaBearCreditedChildren = 0;
    // Additive damage bonus applied in checkVillain() to whatever the
    // character's itemDamage currently is. MOM, BRO, and DAD all
    // raise this; the mechanism is generic so other future powers
    // can share it without further changes.
    int strengthBonus = 0;

protected:
    int gamepad_index = -1;

    // Helpers used by onGamepadEvent to keep its big switch readable.
    void pressDirection(SpriteAnimation* anim,
                        float attackRotation,
                        sf::Vector2f attackPos,
                        sf::Vector2f dir,
                        bool blocked);
    void openHeldClue();

    // Base attributes
    double speed = 120;
    RoomGroup* g = nullptr;
    EntityGroup* entity_group = nullptr;
    // Non-owning pointers into ResourceManager's central texture cache.
    // The cache outlives every Character/Villain (it's a process-wide
    // static and is never cleared while gameplay is running), so the
    // pointers remain valid for the object's lifetime. Sharing one
    // texture across all players also avoids the per-instance GPU
    // upload the old `sf::Texture` member required.
    sf::Texture* sprite_map = nullptr;
    sf::Texture* death_map = nullptr;
    hh::Sound chara_hurt;
    hh::Sound chara_death;
    hh::Sound ghost_sound;
    // An attack animation
    SpriteAnimation attack_anim;
    // The current animation
    SpriteAnimation* curr = nullptr;
    // create 4 sprite animations representing walking
    // in the 4 cardinal directions
    SpriteAnimation walk_up;
    SpriteAnimation walk_down;
    SpriteAnimation walk_left;
    SpriteAnimation walk_right;
    bool stopUp = false;
    bool stopDown = false;
    bool stopLeft = false;
    bool stopRight = false;
    SpriteAnimation death_animation;
    sf::Clock clock;
    bool isStarted = false;
    bool isAlive = true;
    bool isAttacking = false;
};

#endif
