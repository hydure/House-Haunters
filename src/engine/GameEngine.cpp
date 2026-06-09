#include <algorithm>
#include <iostream>
#include <string>
#include <typeinfo>    // std::bad_cast
#include "engine/GameEngine.hpp"
#include "engine/NetworkManager.hpp"
#include "engine/Env.hpp"

void GameEngine::start()
{
    bool ready = false;
    // Find and initialize gamepads
    int gpcount = gpcontroller.addGamepads();
    std::cout << gpcount << " Gamepads Found" << std::endl;
    // Forward change_screen events to changeGameScreen(). Held in an
    // EventSubscription member so ~GameEngine removes it automatically.
    changeScreenSub = EventSubscription(
        Events::addEventListener("change_screen", [this](base_event_type event){
            auto e = dynamic_cast< Event<std::string>& >(*event);
            this->changeGameScreen(e.data);
        }));
    this->init();

    // Allow starting fullscreen via env var (HH_FULLSCREEN=1). The
    // subclass init() may have already called setFullscreen() based on
    // Config; the env var overrides only when explicitly set to 1/true.
    if (Env::asBool("HH_FULLSCREEN")) {
        fullscreen = true;
    }

    // Build the offscreen logical canvas exactly once. From this point
    // on every scene draw goes through frameBuffer, which is winDim-sized
    // regardless of fullscreen state -- that's what lets per-screen code
    // keep using 720x480 coordinates.
    if (!frameBufferReady) {
        if (frameBuffer.create(static_cast<unsigned int>(this->winDim.width),
                               static_cast<unsigned int>(this->winDim.height))) {
            frameBufferReady = true;
            // Crisp pixel-art-style scaling; flip off if you prefer
            // bilinear upscaling in fullscreen.
            frameBuffer.setSmooth(false);
        }
        else {
            std::cout << "Failed to create offscreen frame buffer "
                      << winDim.width << "x" << winDim.height
                      << "; falling back to direct window rendering." << std::endl;
        }
    }
    this->recreateWindow();
    this->running = true;

    // HH_SCREENSHOT=N tells the engine to dump diagnostic PNGs after N
    // rendered frames and then exit. Used by the test harness to verify
    // that the offscreen RenderTexture path is producing the expected
    // pixels without requiring a human to look at the window.
    int screenshotAfterFrames = 0;
    {
        std::string v;
        if (Env::tryGet("HH_SCREENSHOT", &v)) {
            try { screenshotAfterFrames = std::stoi(v); }
            catch (...) { screenshotAfterFrames = 0; }
        }
    }
    std::string screenshotPrefix = "hh_dump";
    {
        std::string v;
        if (Env::tryGet("HH_SCREENSHOT_PREFIX", &v)) {
            screenshotPrefix = v;
        }
    }
    int drawnFrames = 0;

    sf::Clock clock;
    sf::Time timeSinceLastUpdate = sf::Time::Zero;
    // Frame budget derived from the configured target FPS.
    const sf::Time timePerFrame = sf::seconds(1.f / static_cast<float>(targetFps));

    while (window.isOpen()) {
        sf::Time dt = clock.restart();
        timeSinceLastUpdate += dt;
        this->handleEvents();
        while (timeSinceLastUpdate > timePerFrame) {
            ready = true;
            timeSinceLastUpdate -= timePerFrame;
            this->update(timePerFrame.asSeconds());
        }
        // Avoid drawing before the first update tick fires.
        if (ready) {
            this->draw();
            ++drawnFrames;
            if (screenshotAfterFrames > 0 && drawnFrames == screenshotAfterFrames) {
                // Dump frameBuffer contents (what the engine THINKS the
                // scene rendered to) and window contents (what's actually
                // on screen). Compare to diagnose the rendering chain.
                std::cout << "[HH_SCREENSHOT] dumping at frame " << drawnFrames
                          << "; winSize=" << window.getSize().x << "x" << window.getSize().y
                          << " winDim=" << winDim.width << "x" << winDim.height
                          << " fullscreen=" << (fullscreen ? "yes" : "no")
                          << " frameBufferReady=" << (frameBufferReady ? "yes" : "no")
                          << std::endl;
                if (frameBufferReady) {
                    sf::Image img = frameBuffer.getTexture().copyToImage();
                    std::string path = screenshotPrefix + "_framebuffer.png";
                    if (img.saveToFile(path)) {
                        std::cout << "[HH_SCREENSHOT] saved " << path
                                  << " (" << img.getSize().x << "x" << img.getSize().y << ")" << std::endl;
                    }
                }
                // Capture window contents via a Texture readback (the
                // RenderWindow::capture() shortcut was removed in 2.6).
                sf::Vector2u ws = window.getSize();
                sf::Texture cap;
                if (cap.create(ws.x, ws.y)) {
                    cap.update(window);
                    sf::Image wimg = cap.copyToImage();
                    std::string path = screenshotPrefix + "_window.png";
                    if (wimg.saveToFile(path)) {
                        std::cout << "[HH_SCREENSHOT] saved " << path
                                  << " (" << wimg.getSize().x << "x" << wimg.getSize().y << ")" << std::endl;
                    }
                }
                this->exit();
            }
        }
    }
    this->running = false;
}

void GameEngine::update(float dt)
{
    gpcontroller.update();
    // Lockstep step: in OFFLINE mode this is a no-op. In HOST/CLIENT mode
    // it ships the local input edges (buffered by Gamepad via
    // NetworkManager::interceptLocalGamepad), blocks for the merged peer
    // bundle, and queues each merged edge on the Events bus so the
    // currScene->update below sees the same input on every machine.
    NetworkManager::instance().tickStep();
    if (this->currScene) {
        this->currScene->update(dt);
    }
}

void GameEngine::draw()
{
    // Always render through the offscreen frameBuffer (logical 720x480
    // canvas) and then blit it scaled+letterboxed into the window. At
    // native size the scale is 1.0 and position is (0,0), so it's
    // pixel-identical to direct-to-window rendering. Going through one
    // path instead of two avoids subtle window-view state differences
    // that left the native-size windowed mode rendering only the HUD
    // (white background, hearts only) while fullscreen worked fine.
    //
    // If the RenderTexture failed to allocate at startup, fall back to
    // direct-to-window so the game still runs (no fullscreen scaling).
    if (frameBufferReady) {
        frameBuffer.clear(sf::Color::Black);
        if (this->currScene) {
            frameBuffer.draw(*(this->currScene));
        }
        frameBuffer.display();

        window.clear(sf::Color::Black);
        // Force a pixel-mapped view BEFORE blitting the letterbox: SFML
        // 2.x does not auto-resize the default view when the OS resizes
        // the window, so getDefaultView() still reports the original
        // 720x480 size and frameSprite math (computed in pixels by
        // layoutFrameSprite) would get re-stretched by the leftover view.
        // Setting an explicit view sized to the current pixel area means
        // frameSprite.setPosition / setScale read as literal pixels.
        sf::Vector2u winSizeNow = window.getSize();
        window.setView(sf::View(sf::FloatRect(
            0.f, 0.f,
            static_cast<float>(winSizeNow.x),
            static_cast<float>(winSizeNow.y))));
        window.draw(frameSprite);
        window.display();
    }
    else {
        window.clear(sf::Color::Black);
        if (this->currScene) {
            window.draw(*(this->currScene));
        }
        window.display();
    }
}

void GameEngine::addGameScreen(std::string id, std::unique_ptr<GameScreen> s)
{
    s->setEngine(this);
    screens[std::move(id)] = std::move(s);
}

void GameEngine::changeGameScreen(std::string s)
{
    bool canChange = true;
    if (this->currScene) {
        canChange = this->currScene->onExit();
    }
    if (!canChange) {
        return;
    }
    auto it = screens.find(s);
    if (it == screens.end()) {
        std::cout << "Unknown screen: " << s << std::endl;
        return;
    }
    // Release any listeners the outgoing screen registered through subscribe().
    if (this->currScene) {
        this->currScene->clearSubscriptions();
    }
    this->currScene = it->second.get();
    if (this->currScene) {
        this->currScene->init();
    }
}

void GameEngine::handleEvents()
{
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            this->exit();
        }
        else if (event.type == sf::Event::KeyPressed) {
            // F11 toggles fullscreen. Alt+Enter does the same (the
            // Windows-classic combo). Both are consumed at the engine
            // layer; Gamepad polls keyboard state directly and never
            // looks at F11/Enter, so nothing else cares.
            const bool isAlt = event.key.alt;
            if (event.key.code == sf::Keyboard::F11
                || (isAlt && event.key.code == sf::Keyboard::Return))
            {
                this->toggleFullscreen();
            }

            // Edge-triggered "text-style" input events for screens that
            // want to capture raw keystrokes (e.g. the Join Game screen
            // typing in a numeric code). The Gamepad layer abstracts
            // keys into A/B/UP/DOWN/... and never fires per-press
            // events, so without this hook the keyboard couldn't type
            // characters that don't map to a gamepad button. The
            // payload is a tiny string identifier:
            //   "0".."9"  - decimal digit (number row or numpad)
            //   "BACK"    - Backspace
            //   "ENTER"   - Return key (only when no Alt modifier; the
            //               Alt+Enter combo is swallowed above)
            //   "ESC"     - Escape
            //
            // Listeners should treat unknown payloads as no-op so we
            // can add more keys later without breaking older screens.
            const sf::Keyboard::Key k = event.key.code;
            std::string payload;
            if (k >= sf::Keyboard::Num0 && k <= sf::Keyboard::Num9) {
                payload = std::string(1, static_cast<char>('0' + (k - sf::Keyboard::Num0)));
            }
            else if (k >= sf::Keyboard::Numpad0 && k <= sf::Keyboard::Numpad9) {
                payload = std::string(1, static_cast<char>('0' + (k - sf::Keyboard::Numpad0)));
            }
            else if (k == sf::Keyboard::BackSpace) {
                payload = "BACK";
            }
            else if (k == sf::Keyboard::Return && !isAlt) {
                payload = "ENTER";
            }
            else if (k == sf::Keyboard::Escape) {
                payload = "ESC";
            }
            if (!payload.empty()) {
                auto evt = std::make_shared< Event<std::string> >(std::move(payload));
                Events::triggerEvent("key_input", evt);
            }
        }
        else if (event.type == sf::Event::Resized) {
            // SFML 2.x does NOT auto-update the default view when the OS
            // resizes the window. Without this, the previous-frame view
            // (sized to the old client area) gets stretched to fit the
            // new pixel dimensions, which is what made the maximize
            // button produce a half-black / half-white window with
            // mis-positioned HUD elements. Resetting the default view to
            // match the new pixel size keeps frameSprite math in real
            // pixels, then layoutFrameSprite recomputes the letterbox.
            sf::View v(sf::FloatRect(0.f, 0.f,
                                     static_cast<float>(event.size.width),
                                     static_cast<float>(event.size.height)));
            this->window.setView(v);
            this->layoutFrameSprite();
        }
    }
    // Notify of all events that took place last frame.
    Events::notify();
}

// Gives a subclass a chance to prevent the game from exiting and/or do
// cleanup before the window closes.
void GameEngine::exit()
{
    if (this->onExit()) {
        this->window.close();
    }
}

void GameEngine::setFullscreen(bool on)
{
    if (this->fullscreen == on) {
        return;
    }
    this->fullscreen = on;
    // If the window hasn't been created yet (called from subclass
    // init() before start() opens it) just record the flag; start()
    // will pick it up via recreateWindow().
    if (this->window.isOpen() || this->running) {
        this->recreateWindow();
    }
}

void GameEngine::recreateWindow()
{
    sf::VideoMode mode;
    sf::Uint32 style;
    if (this->fullscreen) {
        // Borderless fullscreen at the desktop's current resolution.
        // Style::None gives us a frameless window the size of the
        // primary display -- preferred over Style::Fullscreen because
        // it doesn't change the desktop video mode (no flicker, no
        // window rearrange when toggling, and Alt+Tab stays fast).
        mode = sf::VideoMode::getDesktopMode();
        style = sf::Style::None;
    }
    else {
        mode = sf::VideoMode(static_cast<unsigned int>(this->winDim.width),
                             static_cast<unsigned int>(this->winDim.height));
        // Titlebar + Close + Resize: Resize is what enables the Maximize
        // button in the Windows title bar. Without it the button is
        // permanently grayed out. When the user maximizes (or otherwise
        // resizes) we get an sf::Event::Resized and switch into the
        // letterboxed RenderTexture path automatically.
        style = sf::Style::Titlebar | sf::Style::Close | sf::Style::Resize;
    }
    this->window.create(mode, this->name, style);
    if (this->fullscreen) {
        // Pin to (0,0) on the primary monitor so the borderless window
        // covers the desktop even if the previous windowed position
        // was on a secondary display.
        this->window.setPosition(sf::Vector2i(0, 0));
    }
    this->layoutFrameSprite();
}

void GameEngine::layoutFrameSprite()
{
    if (!frameBufferReady) {
        return;
    }
    frameSprite.setTexture(frameBuffer.getTexture(), true);
    sf::Vector2u winSize = window.getSize();
    if (winSize.x == 0 || winSize.y == 0) {
        return;
    }
    const float canvasW = static_cast<float>(winDim.width);
    const float canvasH = static_cast<float>(winDim.height);
    // Uniform scale that preserves aspect ratio: pick whichever axis
    // saturates first, then center the canvas in the remaining space
    // (the unfilled bars stay black from window.clear()).
    const float scale = std::min(static_cast<float>(winSize.x) / canvasW,
                                 static_cast<float>(winSize.y) / canvasH);
    frameSprite.setScale(scale, scale);
    const float drawW = canvasW * scale;
    const float drawH = canvasH * scale;
    frameSprite.setPosition((static_cast<float>(winSize.x) - drawW) * 0.5f,
                            (static_cast<float>(winSize.y) - drawH) * 0.5f);
}
