#ifndef GAME_SCREEN_HPP
#define GAME_SCREEN_HPP

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include "game/Config.hpp"
#include "engine/GameObject.hpp"
#include "engine/EventManager.hpp"

class GameEngine;

//////////////////////////////
// GameScreen.hpp
//
// Think of GameScreens as game states and the GameEngine as a game-states manager.
//
// Screens are persistent: the engine keeps a single instance of each one and
// re-runs init() on entry. Use subscribe() instead of Events::addEventListener
// so the listener is automatically removed when the engine switches away from
// this screen (or when the screen is destroyed).
//
/////////////////////////////
class GameScreen : public GameObject
{
public:
    virtual ~GameScreen() = default;
    virtual void init() {}
    virtual bool onExit() { return true; }
    void setEngine(GameEngine* e) { this->engine = e; }
    void setConfig(std::shared_ptr<Config> c) { config = std::move(c); }
    // Releases every listener registered through subscribe(). Called by the
    // engine when leaving this screen.
    void clearSubscriptions() { subscriptions.clear(); }
    std::string screenID;
protected:
    std::shared_ptr<Config> config;
    GameEngine* engine = nullptr;
    std::vector<EventSubscription> subscriptions;

    // RAII wrapper around Events::addEventListener. The screen owns the
    // subscription for its lifetime; clearSubscriptions() releases it.
    void subscribe(std::string type, std::function<void (base_event_type)> listener)
    {
        long id = Events::addEventListener(std::move(type), std::move(listener));
        subscriptions.emplace_back(id);
    }
};

#include "engine/GameEngine.hpp"

#endif
