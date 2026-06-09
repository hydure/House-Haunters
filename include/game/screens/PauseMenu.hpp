#ifndef PAUSE_MENU_HPP
#define PAUSE_MENU_HPP

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

#include "engine/Engine.hpp"

//////////////////////////////
// PauseMenu.hpp
//
// In-game pause menu owned by GameplayScreen. Not a GameScreen itself,
// because switching screens would tear down the active gameplay state
// (GameplayScreen::onExit clears views and entities). Instead the
// GameplayScreen freezes its update loop while isOpen() is true and
// draws the menu overlay on top of the (paused) scene.
//
// Pages: MAIN -> {CONTROLS, SETTINGS}. Each page handles UP/DOWN to
// navigate, A/START to confirm, B to go back, and the MENU button to
// close the whole overlay (resume play).
//////////////////////////////
class GameEngine;

class PauseMenu
{
public:
    // Allocates the SFML resources used by the menu (font, text, shapes).
    // Safe to call multiple times; subsequent calls reset state.
    void init(GameEngine* engine);

    // Show / hide the overlay. open() resets the cursor back to the top
    // of the MAIN page so the menu always opens in a known state.
    void open();
    void close();
    bool isOpen() const { return state != State::CLOSED; }

    // Tells the menu whether any active player is currently controlling
    // BRO. Used to hide the "BRO sprint" line on the CONTROLS page when
    // no one in the party can use that binding -- avoids advertising a
    // control that does nothing for the current player(s). Should be
    // refreshed before each open() so the page reflects the live roster.
    void setBroActive(bool active) { broActive = active; }

    // Forward a gamepad/keyboard input. The menu only acts on RELEASED
    // edges to match the convention used by the other screens. Returns
    // true if the event was consumed by the menu (callers should not
    // then forward it to the game).
    bool handleEvent(const GamepadEvent& e);

    // Drawn at logical 720x480 (so menu pixels map to the canvas
    // regardless of any view the gameplay code may have set).
    void draw(sf::RenderTarget& target) const;

    // True while the menu is suppressing gameplay input. Polled by the
    // PlayerView gamepad listener so character movement / actions stop
    // immediately when the menu opens (and resume on close).
    static bool inputBlocked() { return s_inputBlocked; }

private:
    enum class State { CLOSED, MAIN, CONTROLS, SETTINGS };

    void enterState(State s);
    void confirmMain();
    void changeSetting(int delta); // -1 for LEFT, +1 for RIGHT

    void buildMainPage();
    void buildControlsPage();
    void buildSettingsPage();
    void refreshSelection();
    void refreshSettingsLabels();

    // Apply a master-volume change immediately and clamp to [0, 100].
    void setMasterVolume(float v);

    State state = State::CLOSED;
    GameEngine* engine = nullptr;

    // Cached cursor for the MAIN page so re-entering main from a sub-page
    // (e.g. returning from CONTROLS) keeps the user on the same item.
    int mainCursor = 0;
    int settingsCursor = 0;
    bool ready = false;

    // Persistent visual elements.
    sf::RectangleShape backdrop;     // dimming layer over the gameplay
    sf::RectangleShape panel;        // the dialog box itself
    sf::Text title;
    sf::Text footer;

    // Per-page text lines (rebuilt when entering a state). Selected
    // line gets a brighter color via refreshSelection().
    std::vector<sf::Text> lines;

    // Cached master volume so the slider doesn't have to roundtrip
    // through sf::Listener every frame.
    float masterVolume = 100.f;

    // Whether any character in play is BRO -- controls whether the
    // CONTROLS page advertises the BRO-only sprint binding. Set by
    // GameplayScreen via setBroActive() prior to opening the menu.
    bool broActive = false;

    // Shared with PlayerView listeners (and any future gameplay
    // subsystem) so they can skip processing while paused. Static so
    // there's exactly one source of truth across the program; the
    // PauseMenu instance is the only writer.
    static bool s_inputBlocked;
};

#endif
