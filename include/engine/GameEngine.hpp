#ifndef GAME_ENGINE_HPP
#define GAME_ENGINE_HPP

#include <SFML/Graphics.hpp>
#include <memory>
#include <string>
#include <vector>
#include <map>

#include "engine/GameObject.hpp"
#include "engine/GameScreen.hpp"
#include "engine/EventManager.hpp"
#include "engine/Gamepad.hpp"

// Game-state manager: owns the window, the gamepad controller, and the
// collection of registered GameScreens.
class GameEngine
{
public:
    void start();
    void stop(); // pause
    void exit(); // exit the game

    void update(float dt);
    void draw();

    void changeGameScreen(std::string s);
    void addGameScreen(std::string id, std::unique_ptr<GameScreen> s);
    void setDebugMode(bool m) { isDebugMode = m; }

    void setWindowRect(sf::IntRect dim) { winDim = dim; }
    void setWindowRect(int t, int l, int w, int h) { winDim = sf::IntRect(t, l, w, h); }

    // Fullscreen control. The engine renders the scene to a fixed
    // winDim-sized RenderTexture (the "logical canvas") and then blits
    // the texture letterboxed into the window, so screens never see the
    // real window size. setFullscreen / toggleFullscreen recreate the
    // SFML window in borderless-fullscreen (Style::None at the desktop
    // resolution) or back to a normal 720x480 window. F11 and Alt+Enter
    // also toggle at runtime (handled inside GameEngine::handleEvents).
    void setFullscreen(bool on);
    void toggleFullscreen() { setFullscreen(!fullscreen); }
    bool isFullscreen() const { return fullscreen; }

    void setName(const std::string& n) { this->name = n; }

    // Frame-rate cap. Defaults to 60 fps. Pass a non-positive value to fall back to 60.
    void setTargetFps(int fps) { targetFps = (fps > 0) ? fps : 60; }
    int  getTargetFps() const { return targetFps; }

    bool isRunning() const { return running; }

    sf::RenderWindow* getContext() { return &window; }

private:
    bool running = false;
    bool isDebugMode = false;
    bool fullscreen = false;
    int  targetFps = 60;
    GamepadController gpcontroller;
    sf::IntRect winDim;
    sf::RenderWindow window;
    // Fixed-resolution "logical canvas" the entire game renders into.
    // Sized to winDim in start(); never resized after creation. The
    // engine blits this texture letterboxed into the window each frame,
    // which is what enables fullscreen without per-screen rework.
    sf::RenderTexture frameBuffer;
    sf::Sprite frameSprite;
    bool frameBufferReady = false;
    std::string name = "New_Game";
    std::map<std::string, std::unique_ptr<GameScreen>> screens;
    GameScreen* currScene = nullptr;
    // RAII handle for the change_screen listener installed in start().
    EventSubscription changeScreenSub;
    virtual void init() {}                  // aka onStart
    virtual void onUpdate(float /*dt*/) {}
    virtual bool onExit() { return true; }
    void handleEvents();
    virtual void onEvent() {}
    // Window lifecycle helpers used by start() and setFullscreen().
    void recreateWindow();
    void layoutFrameSprite();
};

#endif
