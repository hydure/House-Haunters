#ifndef JOYSTICK_CONTROLS_HPP
#define JOYSTICK_CONTROLS_HPP
///////////
// Gamepad abstraction layer.
//
// Two concerns:
//   * Gamepad             - one physical controller (or the keyboard). Polls
//                           SFML each tick, debounces edges, emits
//                           "gamepad_event" events with a layout-neutral
//                           button name (A, B, X, Y, UP, DOWN, LEFT, RIGHT,
//                           MENU, START, SELECT, L, R) and the controller
//                           index that produced them.
//   * GamepadController   - the bag of every connected controller. Owns
//                           the keyboard slot, detects new physical
//                           controllers being plugged in mid-run, and
//                           emits "gamepad_connect" / "gamepad_disconnect"
//                           events so screens (the character-select lobby
//                           especially) can react to hot-plug.
//
// Layout detection is by USB vendor / product id from
// sf::Joystick::Identification. Unknown ids fall back to a generic SDL-style
// button mapping that covers most modern XInput/HID gamepads.
////////////
#include <string>
#include <map>
#include <vector>
#include <SFML/Graphics.hpp>
#include "engine/EventManager.hpp"
#include "engine/EngineEvents.hpp"

class GamepadEvent : public BasicEvent
{
public:
    enum TYPE { PRESSED, RELEASED, DISCONNECT, CONNECT };
    int index = -1;
    TYPE type = PRESSED;
    std::string button;
};

struct BUTTON_S {
    int button;
    bool isDown;
};

struct KBUTTON_S {
    sf::Keyboard::Key button;
    bool isDown;
};

class Gamepad
{
public:
    Gamepad() = default;
    Gamepad(int index) : controllerIndex(index) { setLayout(guessLayout()); }
    // GENERIC      - any unknown HID gamepad. Uses the SDL-style mapping
    //                (button 0=A, 1=B, 2=X, 3=Y, 4=LB, 5=RB, 6=SELECT, 7=START)
    //                which is the de-facto layout most modern controllers
    //                report when plugged into Windows in XInput mode.
    // PS3 / PS4 / PS5 - DualShock 3 / DualShock 4 / DualSense.
    // XB360, XB1, XBSERIES - Xbox 360, Xbox One, Xbox Series X|S.
    // XBOX_GENERIC - any other XInput-style controller (third-party Xbox
    //                clones, the Steam Controller in XInput mode, etc.).
    // STEAM        - Valve Steam Controller (HID mode, distinct from the
    //                XInput re-route).
    // SWITCH_PRO   - Nintendo Switch Pro Controller (HID, not XInput).
    // EIGHT_BITDO  - 8BitDo SN30/SF30 Pro family; common third-party pad.
    // KEYBOARD     - reserved for the implicit "keyboard counts as one
    //                player" slot assigned by GamepadController.
    enum LAYOUT {
        GENERIC,
        PS3, PS4, PS5,
        XB360, XB1, XBSERIES, XBOX_GENERIC,
        STEAM, SWITCH_PRO, EIGHT_BITDO,
        KEYBOARD
    };

    void setController(int i) { controllerIndex = i; setLayout(guessLayout()); }
    void setIndex(int i) { this->controllerIndex = i; }
    int  getIndex() const { return this->controllerIndex; }
    void setActive(bool a) { this->isActive_b = a; }
    void setConnected(bool c) { this->isConnected_b = c; }

    void setLayout(LAYOUT layout);

    LAYOUT getLayout() const { return layout; }
    bool isConnected() const { return this->isConnected_b; }
    bool isActive() const { return this->isActive_b; }

    void update();
    int playerIndex = -1;

    // Resolves a (vendorId, productId) pair to a layout enum. Public so
    // tests can exercise the detection table without a real joystick.
    static LAYOUT layoutFor(unsigned int vendorId, unsigned int productId);

    // Returns true when this layout maps `name` to a real input source
    // (button or key). Useful for testing whether the layout table is
    // populated for every documented binding.
    bool hasBinding(const std::string& name) const;

protected:
    // Guess the controller layout by checking vendor id / product id.
    LAYOUT guessLayout();
    LAYOUT layout = GENERIC;
    int controllerIndex = -1;
    std::map<std::string, BUTTON_S> button_map;
    std::map<std::string, KBUTTON_S> kbutton_map;
    bool isConnected_b = true;
    bool isActive_b = true;
};

class GamepadController
{
public:
    int addGamepads();                          // Detect connected gamepads and return how many were found.
    void removeGamepad(int id);
    void disableGamepads(std::vector<int> ids); // Disable 0 or more gamepads.
    void enableGamepads(std::vector<int> ids);  // Enable 1 or more gamepads.
    Gamepad* getGamepad(int index);
    int count = 0;
    void update();

    // Walk every SFML joystick slot and reconcile against our internal
    // table. Newly-connected pads get a Gamepad created for them and a
    // "gamepad_connect" event queued; previously-connected pads that
    // dropped raise "gamepad_disconnect". Called once per frame by
    // GamepadController::update(); also exposed publicly so tests can
    // drive the detection logic directly.
    //
    // The keyboard slot (index always equals the number of physical
    // pads at addGamepads() time) is preserved across reconciliation.
    void reconcileConnections();

    // For tests: total registered slots (physical pads + the one keyboard).
    int slotCount() const { return static_cast<int>(gamepads.size()); }
    // For tests: does this index map to a registered pad?
    bool has(int index) const { return gamepads.count(index) > 0; }

private:
    std::map<int, Gamepad> gamepads;
    // Index of the slot that owns the keyboard layout. Reassigned on
    // every reconcileConnections() pass so the keyboard always sits at
    // index == (highest physical slot + 1), keeping config->player_map
    // lookups stable when a single user plays solo on the keyboard
    // even if their controller drops out and reconnects.
    int keyboardSlot = -1;
};

#endif