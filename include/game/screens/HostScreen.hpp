#ifndef HOST_SCREEN_HPP
#define HOST_SCREEN_HPP

#include <string>
#include <vector>
#include <SFML/Graphics.hpp>
#include "engine/Engine.hpp"
#include "engine/ResourceManager.hpp"

//////////////////////////////
// HostScreen.hpp
//
// "Host Game" menu reached from the title screen. Opens the network
// listener in non-blocking mode and waits for one remote peer to join.
// Displays:
//   * The host's LAN IP (e.g. "192.168.1.42")
//   * A 12-digit decimal JOIN CODE -- the IPv4 address with each octet
//     zero-padded to 3 digits. Shown grouped as "192.168.001.042" so
//     it's readable; JoinScreen accepts the raw 12-digit form.
//
// Port is fixed at NetworkManager's default (53353) so the code only
// has to encode 4 IPv4 bytes.
//
// On accept the screen finishes the handshake, primes Config like the
// env-var path in HouseHaunters.cpp does, and jumps to GamePlay.
//
// Pressing B (gamepad) / Esc (keyboard) cancels and returns to the title.
//////////////////////////////
class HostScreen : public GameScreen
{
public:
    void init() override;
    void onUpdate(float dt) override;
    void onDraw(sf::RenderTarget& ctx, sf::RenderStates states) const override;

protected:
    void onGamepadEvent(GamepadEvent e);
    // Raw keyboard hook -- only used for ESC (cancel) so keyboard
    // players can back out without having to map a controller button.
    void onKeyInput(const std::string& key);
    // Builds the displayed strings (IP, code, status).
    void refreshLabels();
    // Networking complete -> set up Config and switch to GamePlay.
    void launchGameplay();
    // Cancels the listener and returns to the title screen.
    void cancelToTitle();

    sf::RectangleShape background;
    sf::Text           title;
    sf::Text           ipLabel;
    sf::Text           codeLabel;
    sf::Text           status;
    sf::Text           footer;

    // Number of remote peers to wait for (1..3 since we cap at 4 players).
    int  expectedPeers = 1;
    // True once beginHostListening succeeded.
    bool listening     = false;
    // True once pollAccept signaled completion (handshake done).
    bool done          = false;
    // Set if anything in the network layer failed; locks the screen
    // until the user cancels.
    bool errored       = false;
    // Swallow the first event so the A press that opened this screen
    // doesn't immediately get re-interpreted here.
    bool ready         = false;
};

#endif
