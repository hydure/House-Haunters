#ifndef GAMEPLAY_SCREEN_HPP
#define GAMEPLAY_SCREEN_HPP

#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>
#include "engine/Engine.hpp"
#include "engine/AudioPlayer.hpp"
#include "game/characters/Character.hpp"
#include "game/characters/Villain.hpp"
#include "game/objects/Clue.hpp"
#include "game/rooms/RoomGroup.hpp"
#include "game/characters/PlayerView.hpp"
#include "game/screens/PauseMenu.hpp"
#include "components/EntityGroup.hpp"

//////////////////////////
// GameplayScreen.hpp
//
// The screen the user is on while actually playing. Drives the rooms,
// entities, and per-player views.
//
// Screen flow (roughly):
//   Title -> Character Select -> Gameplay (this) -> Game Over -> Title
//
// Next check out src/GameplayScreen.cpp
//////////////////////////

class GameplayScreen : public GameScreen
{
public:
    void init() override;
    bool onExit() override;
    void onUpdate(float dt) override;
    void onDraw(sf::RenderTarget& ctx, sf::RenderStates states) const override;

    // True iff a snapshot value should overwrite a freshly-spawned
    // character's HP. A snapshot < 0 means "no snapshot recorded";
    // a snapshot of 0 means the player died in the previous round --
    // restoring that would spawn the new game into an instant Game
    // Over (and trigger the same on every restart). Both cases are
    // skipped here so a new round always starts with the per-class
    // default HP. Exposed publicly + static so the regression test
    // can pin this contract without booting the screen.
    static bool shouldApplyHealthSnapshot(int snap) { return snap > 0; }

protected:
    void createViews(int numPlayers);
    void createClues();
    // Builds a single PlayerView + Character for the given 1-based
    // player number. Optionally calls Character::init() inline (set
    // initInPlace=true for late-join, false at startup so a single
    // entity_group.init() pass owns the call order).
    void spawnPlayerForSlot(int playernum, int numPlayers, bool initInPlace);
    // Walks every live (non-villain) character and overwrites their
    // health with the value recorded by NetworkManager. No-op for any
    // slot that has never had a snapshot recorded.
    void applyHealthSnapshots();

    // Polls NetworkManager for a peer that connected after the game
    // started. Spawns the corresponding player + view, then bumps the
    // local num_players counter so the room grid resize / win-condition
    // book-keeping stays consistent.
    void acceptLateJoiners();
    // Once per tick we push each live character's HP back to
    // NetworkManager so a drop/rejoin restores the exact value they
    // had when they vanished.
    void recordHealthSnapshots();

    int phase = 1;
    int num_players = 1;
    int hiLow = 0;

    // Seconds spent in phase 1 (clue search) before the villain spawns.
    // Computed in init() as config->time_Per_Phase scaled by difficulty
    // (EASY = longer, HARD = shorter). 0 = "not initialized yet".
    float phaseTime = 0.f;

    sf::Clock clock;
    RoomGroup group;
    std::vector< std::unique_ptr<PlayerView> > views;
    std::shared_ptr<Villain> ghost;
    std::unique_ptr<Clue> clue;
    EntityGroup entity_group;
    ClueReader reader;
    hh::Sound hunt;

    // In-game pause menu. Owns its own SFML resources and is drawn over
    // the (frozen) scene whenever pauseMenu.isOpen(). Built once in
    // init() and reused across pause cycles.
    PauseMenu pauseMenu;

    // Bottom-right hint shown during play ("Press ESC for menu"). Built
    // once in init() so onDraw stays allocation-free.
    sf::Text menuHint;
    bool menuHintReady = false;
};

#endif
