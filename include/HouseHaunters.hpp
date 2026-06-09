#ifndef HOUSE_HAUNTERS_HPP
#define HOUSE_HAUNTERS_HPP

#include <memory>
#include "engine/Engine.hpp"
#include "game/screens/GameplayScreen.hpp"
#include "game/screens/GametitleScreen.hpp"
#include "game/screens/CharacterScreen.hpp"
#include "game/screens/ControlsScreen.hpp"
#include "game/screens/EndGameScreen.hpp"
#include "game/screens/GamestoryScreen.hpp"
#include "game/screens/HostScreen.hpp"
#include "game/screens/JoinScreen.hpp"

////////////////////////
// HouseHaunters.hpp
//
// The HouseHaunters game engine.
// Drives window creation/teardown, screen switching, and the main loop.
// All initialization happens in init(), which is called automatically when
// the engine starts.
//
// High-level architecture (three tiers):
//   1. GameEngine (this class) owns the SFML window, the event pump, and
//      the active GameScreen pointer.
//   2. Each GameScreen (title, character select, controls, gameplay,
//      story, end-game) is a self-contained state with its own update/
//      draw/input handling. GameEngine only knows about the current one.
//   3. GameplayScreen composes the two big content owners:
//        - EntityGroup -- owns Characters and Clues
//        - RoomGroup   -- owns Rooms
//      Those two own their contents through typed vectors; they are NOT
//      attached to the generic GameObject scene graph. See the comments
//      at the top of EntityGroup.hpp / RoomGroup.hpp / GameObject.hpp for
//      the full ownership rules.
//
// Next check out src/HouseHaunters.cpp
///////////////////////

class HouseHauntersGame : public GameEngine
{
private:
    void init() override;

protected:
    std::shared_ptr<Config> config;
};

#endif
