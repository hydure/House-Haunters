#ifndef JOIN_SCREEN_HPP
#define JOIN_SCREEN_HPP

#include <array>
#include <string>
#include <vector>
#include <SFML/Graphics.hpp>
#include "engine/Engine.hpp"
#include "engine/ResourceManager.hpp"
#include "game/JoinCode.hpp"

//////////////////////////////
// JoinScreen.hpp
//
// "Join Game" menu reached from the title screen. Lets the player enter
// the 4-digit ROOM CODE printed on the host's HostScreen and uses
// NetworkManager's UDP discovery to resolve the host IP from that code
// before opening the TCP gameplay connection.
//
// Two input paths are supported simultaneously so the screen works for
// keyboard players AND gamepad players without a mode switch:
//
//   Gamepad (LAYOUT != KEYBOARD typically):
//     * LEFT / RIGHT moves the cursor across the 4 slots.
//     * UP / DOWN cycles the digit at the current slot (0..9).
//     * A / START commits the code and attempts to connect (~5s timeout).
//     * B / MENU cancels back to the title screen.
//
//   Keyboard (raw key_input events emitted by GameEngine):
//     * 0-9 (top row or numpad) writes that digit at the cursor and
//       auto-advances to the next slot.
//     * Backspace clears the previous slot and moves the cursor back.
//     * Enter commits the code and attempts to connect.
//     * Esc cancels back to the title screen.
//
// Port is fixed at NetworkManager's default (53353); the discovery
// channel uses a separate UDP port (see NetworkManager.cpp).
//////////////////////////////
class JoinScreen : public GameScreen
{
public:
    void init() override;
    void onUpdate(float dt) override;
    void onDraw(sf::RenderTarget& ctx, sf::RenderStates states) const override;

protected:
    void onGamepadEvent(GamepadEvent e);
    // Direct keyboard digit/control entry (independent of the gamepad
    // button abstraction so 0-9 keys are picked up without remapping).
    void onKeyInput(const std::string& key);
    // Rebuilds the displayed code string + status line.
    void refreshLabels();
    // Validates the entered digits and dispatches the connect.
    void tryConnect();
    void cancelToTitle();
    // Once connected, prime Config and switch to GamePlay.
    void launchGameplay();

    sf::RectangleShape background;
    sf::Text           title;
    sf::Text           prompt;
    // One sf::Text per digit slot so we can color-highlight the
    // active cursor without rebuilding strings every frame.
    std::array<sf::Text, JoinCode::kCodeLen> slots;
    sf::Text           status;
    sf::Text           footer;

    // Decimal digits 0..9 -- displayed as 0-9.
    std::array<int, JoinCode::kCodeLen> digits{};
    int                cursor  = 0;
    // Connection state.
    bool busy    = false;  // currently inside connect attempt (UI feedback)
    bool failed  = false;  // last attempt failed
    bool done    = false;  // success -> transitioning away
    bool ready   = false;  // first-frame input swallow
};

#endif
