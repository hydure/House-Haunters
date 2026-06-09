#ifndef GAME_CONTROLS_SCREEN_HPP
#define GAME_CONTROLS_SCREEN_HPP

#include <vector>
#include <SFML/Graphics.hpp>
#include "engine/Engine.hpp"
#include "engine/ResourceManager.hpp"

//////////////////////////////
// ControlsScreen.hpp
//
// Static reference screen reachable from the title screen (Y to open).
// Lists keyboard + gamepad bindings and the per-character abilities so
// players can check their controls without having to leave the game.
// Any button press goes back to the title.
//////////////////////////////
class ControlsScreen : public GameScreen
{
public:
    void init() override;
    void onDraw(sf::RenderTarget& ctx, sf::RenderStates states) const override;
    void onUpdate(float dt) override;

protected:
    void onGamepadEvent(GamepadEvent e);

    sf::RectangleShape background;
    sf::Text title;
    sf::Text footer;
    // One sf::Text per visible line so we can color-code section headers.
    std::vector<sf::Text> lines;

    // Swallow the very first input so a held button used to open this
    // screen doesn't immediately bounce us back to the title.
    bool ready = false;
};

#endif
