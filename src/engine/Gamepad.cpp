#include <iostream>
#include "engine/Gamepad.hpp"
#include "engine/NetworkManager.hpp"

namespace {
// Emit a PRESSED/RELEASED event when a tracked button transitions.
// In OFFLINE mode the edge goes straight onto the local event bus.
// In HOST/CLIENT mode NetworkManager intercepts the edge, buffers it
// for the current tick, and re-emits it (with a slot-id index) after
// the lockstep round-trip. See include/engine/NetworkManager.hpp.
void dispatchButtonState(const std::string& button, bool isPressed,
                         bool& wasPressed, int controllerIndex)
{
    if (isPressed && !wasPressed) {
        if (!NetworkManager::instance().interceptLocalGamepad(button, true)) {
            auto event = std::make_shared<GamepadEvent>();
            event->button = button;
            event->type   = GamepadEvent::TYPE::PRESSED;
            event->index  = controllerIndex;
            Events::queueEvent("gamepad_event", event);
        }
        wasPressed = true;
    }
    else if (!isPressed && wasPressed) {
        if (!NetworkManager::instance().interceptLocalGamepad(button, false)) {
            auto event = std::make_shared<GamepadEvent>();
            event->button = button;
            event->type   = GamepadEvent::TYPE::RELEASED;
            event->index  = controllerIndex;
            Events::queueEvent("gamepad_event", event);
        }
        wasPressed = false;
    }
}

// XInput-style button mapping. Most modern controllers (Xbox 360, Xbox
// One, Xbox Series, third-party XInput pads, the Steam Controller in
// XInput emulation, 8BitDo pads in XInput mode) report buttons in this
// order under Windows SDL/SFML:
//   0 = A   1 = B   2 = X   3 = Y
//   4 = LB  5 = RB  6 = SELECT(BACK)  7 = START
//   8 = L3  9 = R3
// We expose A/B/X/Y plus a SELECT and L/R alongside the historic 7=START
// binding.
void buildXInputButtons(std::map<std::string, BUTTON_S>& m)
{
    m["A"]      = BUTTON_S{ 0, false};
    m["B"]      = BUTTON_S{ 1, false};
    m["X"]      = BUTTON_S{ 2, false};
    m["Y"]      = BUTTON_S{ 3, false};
    m["L"]      = BUTTON_S{ 4, false};
    m["R"]      = BUTTON_S{ 5, false};
    m["SELECT"] = BUTTON_S{ 6, false};
    m["START"]  = BUTTON_S{ 7, false};
    m["MENU"]   = BUTTON_S{ 7, false}; // alias of START
    // D-pad rides on the POV axis; the cardinal lookups override these.
    m["UP"]    = BUTTON_S{-1, false};
    m["DOWN"]  = BUTTON_S{-1, false};
    m["LEFT"]  = BUTTON_S{-1, false};
    m["RIGHT"] = BUTTON_S{-1, false};
}

// PlayStation HID button mapping. PS3/PS4/PS5 in DirectInput mode report
// face buttons in a different order than XInput; the historic codebase
// shipped a 0=X 1=A 2=B 3=Y mapping for "PS4" which works on most
// DualShock 4 firmware under SFML on Windows. Keep it for PS3 and
// DualSense too -- if a user's specific firmware deviates, they can
// override LAYOUT manually through guessLayout's product-id table.
void buildPlayStationButtons(std::map<std::string, BUTTON_S>& m)
{
    m["X"]      = BUTTON_S{ 0, false}; // Square
    m["A"]      = BUTTON_S{ 1, false}; // Cross  -> "A" (confirm)
    m["B"]      = BUTTON_S{ 2, false}; // Circle -> "B" (cancel)
    m["Y"]      = BUTTON_S{ 3, false}; // Triangle
    m["L"]      = BUTTON_S{ 4, false}; // L1
    m["R"]      = BUTTON_S{ 5, false}; // R1
    m["SELECT"] = BUTTON_S{ 8, false}; // Share / Create
    m["START"]  = BUTTON_S{ 9, false}; // Options
    m["MENU"]   = BUTTON_S{ 9, false}; // alias of START (Options)
    m["UP"]    = BUTTON_S{-1, false};
    m["DOWN"]  = BUTTON_S{-1, false};
    m["LEFT"]  = BUTTON_S{-1, false};
    m["RIGHT"] = BUTTON_S{-1, false};
}

// Nintendo Switch Pro Controller HID layout. Button order on Windows
// SDL/SFML: 0=B 1=A 2=Y 3=X 4=L 5=R 6=ZL 7=ZR 8=- 9=+ 10=L3 11=R3 12=Home 13=Capture.
// We swap A/B and X/Y so the on-screen prompts (which talk about a
// physical "A" position) match Nintendo's button layout convention.
void buildSwitchProButtons(std::map<std::string, BUTTON_S>& m)
{
    m["B"]      = BUTTON_S{ 0, false};
    m["A"]      = BUTTON_S{ 1, false};
    m["Y"]      = BUTTON_S{ 2, false};
    m["X"]      = BUTTON_S{ 3, false};
    m["L"]      = BUTTON_S{ 4, false};
    m["R"]      = BUTTON_S{ 5, false};
    m["SELECT"] = BUTTON_S{ 8, false}; // Minus
    m["START"]  = BUTTON_S{ 9, false}; // Plus
    m["MENU"]   = BUTTON_S{ 9, false};
    m["UP"]    = BUTTON_S{-1, false};
    m["DOWN"]  = BUTTON_S{-1, false};
    m["LEFT"]  = BUTTON_S{-1, false};
    m["RIGHT"] = BUTTON_S{-1, false};
}
} // namespace

void Gamepad::setLayout(LAYOUT lay)
{
    this->layout = lay;
    button_map.clear();
    kbutton_map.clear();
    switch (lay) {
        case LAYOUT::KEYBOARD:
            kbutton_map["A"]      = KBUTTON_S{sf::Keyboard::Key::Z,      false};
            kbutton_map["B"]      = KBUTTON_S{sf::Keyboard::Key::X,      false};
            kbutton_map["X"]      = KBUTTON_S{sf::Keyboard::Key::C,      false};
            kbutton_map["Y"]      = KBUTTON_S{sf::Keyboard::Key::V,      false};
            kbutton_map["UP"]     = KBUTTON_S{sf::Keyboard::Key::Up,     false};
            kbutton_map["LEFT"]   = KBUTTON_S{sf::Keyboard::Key::Left,   false};
            kbutton_map["RIGHT"]  = KBUTTON_S{sf::Keyboard::Key::Right,  false};
            kbutton_map["DOWN"]   = KBUTTON_S{sf::Keyboard::Key::Down,   false};
            // MENU opens the in-game pause menu (Resume / Controls /
            // Settings / Exit / Quit). Distinct binding so gameplay
            // bindings (Z/X/C/V) keep their meanings.
            kbutton_map["MENU"]   = KBUTTON_S{sf::Keyboard::Key::Escape, false};
            kbutton_map["START"]  = KBUTTON_S{sf::Keyboard::Key::Return, false};
            kbutton_map["SELECT"] = KBUTTON_S{sf::Keyboard::Key::Tab,    false};
            kbutton_map["L"]      = KBUTTON_S{sf::Keyboard::Key::Q,      false};
            kbutton_map["R"]      = KBUTTON_S{sf::Keyboard::Key::E,      false};
            break;
        case LAYOUT::PS3:
        case LAYOUT::PS4:
        case LAYOUT::PS5:
            buildPlayStationButtons(button_map);
            break;
        case LAYOUT::SWITCH_PRO:
            buildSwitchProButtons(button_map);
            break;
        case LAYOUT::XB360:
        case LAYOUT::XB1:
        case LAYOUT::XBSERIES:
        case LAYOUT::XBOX_GENERIC:
        case LAYOUT::STEAM:
        case LAYOUT::EIGHT_BITDO:
        case LAYOUT::GENERIC:
        default:
            // Steam Controller, 8BitDo (in XInput mode), and any
            // unrecognized HID gamepad all default to the SDL XInput
            // mapping, which is the most likely layout to "just work"
            // on Windows. Users with a deviant controller can extend
            // layoutFor() with a vendor / product id case below.
            buildXInputButtons(button_map);
            break;
    }
}

bool Gamepad::hasBinding(const std::string& name) const
{
    if (layout == LAYOUT::KEYBOARD) {
        return kbutton_map.count(name) > 0;
    }
    return button_map.count(name) > 0;
}

Gamepad::LAYOUT Gamepad::layoutFor(unsigned int vendorId, unsigned int productId)
{
    // Vendor IDs are USB-IF assigned. Product IDs are vendor-internal.
    // When in doubt prefer the more specific case; the generic family
    // fallbacks (XBOX_GENERIC, PS4) cover unrecognized product ids.
    switch (vendorId) {
        case 0x054c: // Sony Interactive Entertainment
            switch (productId) {
                case 0x0268: return LAYOUT::PS3;            // DualShock 3
                case 0x05c4: return LAYOUT::PS4;            // DualShock 4 v1
                case 0x09cc: return LAYOUT::PS4;            // DualShock 4 v2
                case 0x0ba0: return LAYOUT::PS4;            // DualShock 4 USB wireless adapter
                case 0x0ce6: return LAYOUT::PS5;            // DualSense
                case 0x0df2: return LAYOUT::PS5;            // DualSense Edge
                default:     return LAYOUT::PS4;
            }
        case 0x045e: // Microsoft
            switch (productId) {
                case 0x028e: return LAYOUT::XB360;          // Xbox 360 wired
                case 0x028f: return LAYOUT::XB360;          // Xbox 360 wireless
                case 0x02d1: return LAYOUT::XB1;            // Xbox One
                case 0x02dd: return LAYOUT::XB1;            // Xbox One (1st-gen FW upd)
                case 0x02e0: return LAYOUT::XB1;            // Xbox One S BT
                case 0x02e3: return LAYOUT::XB1;            // Xbox One Elite
                case 0x02ea: return LAYOUT::XB1;            // Xbox One S
                case 0x02fd: return LAYOUT::XB1;            // Xbox One S BT (alt rev)
                case 0x0b00: return LAYOUT::XBSERIES;       // Xbox Elite Series 2
                case 0x0b05: return LAYOUT::XBSERIES;       // Xbox Elite Series 2 BT
                case 0x0b12: return LAYOUT::XBSERIES;       // Xbox Series X|S wired
                case 0x0b13: return LAYOUT::XBSERIES;       // Xbox Series X|S BT
                case 0x0b20: return LAYOUT::XBSERIES;       // Xbox Adaptive Controller
                default:     return LAYOUT::XBOX_GENERIC;
            }
        case 0x28de: // Valve
            switch (productId) {
                case 0x1142: return LAYOUT::STEAM;          // Steam Controller wired
                case 0x1102: return LAYOUT::STEAM;          // Steam Controller wireless dongle
                case 0x1201: return LAYOUT::STEAM;          // Steam Controller (alt rev)
                default:     return LAYOUT::STEAM;
            }
        case 0x057e: // Nintendo
            switch (productId) {
                case 0x2009: return LAYOUT::SWITCH_PRO;     // Switch Pro Controller
                case 0x2017: return LAYOUT::SWITCH_PRO;     // Switch SNES controller
                case 0x2006: return LAYOUT::SWITCH_PRO;     // Joy-Con (L)
                case 0x2007: return LAYOUT::SWITCH_PRO;     // Joy-Con (R)
                default:     return LAYOUT::SWITCH_PRO;
            }
        case 0x2dc8: // 8BitDo
            return LAYOUT::EIGHT_BITDO;
        case 0x0079: // DragonRise / generic clones (commonly used by NES-style USB pads)
        case 0x0e6f: // PDP (third-party Xbox controllers)
        case 0x24c6: // PowerA (third-party Xbox controllers)
        case 0x1532: // Razer (third-party Xbox controllers)
            return LAYOUT::XBOX_GENERIC;
        default:
            return LAYOUT::GENERIC;
    }
}

Gamepad::LAYOUT Gamepad::guessLayout()
{
    if (controllerIndex < 0) {
        return LAYOUT::GENERIC;
    }
    sf::Joystick::Identification id = sf::Joystick::getIdentification(controllerIndex);
    LAYOUT lay = layoutFor(id.vendorId, id.productId);
    std::cout << "Gamepad slot " << controllerIndex
              << " VID=0x" << std::hex << id.vendorId
              << " PID=0x" << id.productId << std::dec
              << " -> layout " << static_cast<int>(lay) << std::endl;
    return lay;
}

void Gamepad::update()
{
    if (this->layout == LAYOUT::KEYBOARD) {
        for (auto& kv : kbutton_map) {
            bool isPressed = sf::Keyboard::isKeyPressed(kv.second.button);
            dispatchButtonState(kv.first, isPressed, kv.second.isDown, controllerIndex);
        }
        return;
    }

    if (!sf::Joystick::isConnected(controllerIndex)) {
        if (this->isConnected()) {
            std::cout << "CONTROLLER DISCONNECTED" << std::endl;
            this->isConnected_b = false;
        }
        return;
    }

    if (!this->isConnected()) {
        this->isConnected_b = true;
        std::cout << "CONTROLLER CONNECTED AT INDEX " << controllerIndex << std::endl;
    }

    // SFML treats the d-pad as an axis.
    const float povx = sf::Joystick::getAxisPosition(controllerIndex, sf::Joystick::PovX);
    const float povy = sf::Joystick::getAxisPosition(controllerIndex, sf::Joystick::PovY);

    for (auto& kv : button_map) {
        const std::string& button = kv.first;
        auto& state = kv.second;
        bool isPressed = false;
        if      (button == "UP"    && povy == -100) isPressed = true;
        else if (button == "DOWN"  && povy ==  100) isPressed = true;
        else if (button == "LEFT"  && povx == -100) isPressed = true;
        else if (button == "RIGHT" && povx ==  100) isPressed = true;
        else if (state.button >= 0) {
            isPressed = sf::Joystick::isButtonPressed(controllerIndex, state.button);
        }
        dispatchButtonState(button, isPressed, state.isDown, controllerIndex);
    }
}

Gamepad* GamepadController::getGamepad(int index)
{
    auto it = gamepads.find(index);
    return it == gamepads.end() ? nullptr : &it->second;
}

int GamepadController::addGamepads()
{
    sf::Joystick::update();
    std::cout << "Searching for Gamepads..." << std::endl;
    int found = 0;
    for (int i = 0; i < static_cast<int>(sf::Joystick::Count); i++) {
        if (sf::Joystick::isConnected(i)) {
            std::cout << "Gamepad found at index " << i << std::endl;
            gamepads[i] = Gamepad(i);
            found++;
        }
    }

    // Reserve the next slot for keyboard input.
    keyboardSlot = found;
    gamepads[keyboardSlot] = Gamepad();
    gamepads[keyboardSlot].setIndex(keyboardSlot);
    gamepads[keyboardSlot].setLayout(Gamepad::LAYOUT::KEYBOARD);
    this->count = found;
    return found;
}

void GamepadController::reconcileConnections()
{
    sf::Joystick::update();

    // Detect newly-connected physical pads in the SFML range. We only
    // walk the SFML joystick slots; the keyboard slot is handled
    // separately so we never accidentally treat it as a dropped pad.
    for (int i = 0; i < static_cast<int>(sf::Joystick::Count); ++i) {
        const bool sfmlConnected = sf::Joystick::isConnected(i);
        const bool tracked       = gamepads.count(i) > 0
                                && gamepads[i].getLayout() != Gamepad::LAYOUT::KEYBOARD;

        if (sfmlConnected && !tracked) {
            // Brand-new pad -- or a pad that re-appeared after being
            // dropped. Build a Gamepad in place so its button_map is
            // populated and its layout is guessed from the current
            // VID/PID. Bump the count so screens know how many
            // physical pads are live.
            gamepads[i] = Gamepad(i);
            ++count;
            auto evt = std::make_shared<GamepadEvent>();
            evt->index  = i;
            evt->type   = GamepadEvent::TYPE::CONNECT;
            evt->button = ""; // unused for CONNECT/DISCONNECT
            Events::queueEvent("gamepad_connect", evt);
        }
        else if (!sfmlConnected && tracked) {
            // Pad we used to know about is gone. Emit a disconnect
            // event so screens can de-assign that controller's
            // player_map slot. Keep the Gamepad object around so the
            // index keeps resolving (a re-plug will reuse the slot).
            gamepads[i].setConnected(false);
            --count;
            if (count < 0) count = 0;
            auto evt = std::make_shared<GamepadEvent>();
            evt->index  = i;
            evt->type   = GamepadEvent::TYPE::DISCONNECT;
            evt->button = "";
            Events::queueEvent("gamepad_disconnect", evt);
        }
    }

    // Re-park the keyboard slot at (highest physical index + 1) so it
    // always sits past the SFML joystick range and never collides with
    // a hot-plugged pad. If the keyboard slot moved, swap the Gamepad
    // record over.
    int desiredKb = 0;
    for (auto& kv : gamepads) {
        if (kv.second.getLayout() == Gamepad::LAYOUT::KEYBOARD) continue;
        if (kv.first >= desiredKb) desiredKb = kv.first + 1;
    }
    if (desiredKb != keyboardSlot && keyboardSlot >= 0) {
        Gamepad kb = gamepads[keyboardSlot];
        gamepads.erase(keyboardSlot);
        kb.setIndex(desiredKb);
        kb.setLayout(Gamepad::LAYOUT::KEYBOARD);
        gamepads[desiredKb] = kb;
        keyboardSlot = desiredKb;
    }
}

void GamepadController::removeGamepad(int id)
{
    auto it = gamepads.find(id);
    if (it == gamepads.end()) return;
    const bool wasPhysical = (it->second.getLayout() != Gamepad::LAYOUT::KEYBOARD);
    gamepads.erase(it);
    if (wasPhysical && count > 0) --count;
}

void GamepadController::disableGamepads(std::vector<int> ids)
{
    for (int id : ids) {
        auto it = gamepads.find(id);
        if (it != gamepads.end()) it->second.setActive(false);
    }
}

void GamepadController::enableGamepads(std::vector<int> ids)
{
    for (int id : ids) {
        auto it = gamepads.find(id);
        if (it != gamepads.end()) it->second.setActive(true);
    }
}

void GamepadController::update()
{
    reconcileConnections();
    for (auto& kv : gamepads) {
        kv.second.update();
    }
}