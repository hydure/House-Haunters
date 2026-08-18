#ifndef END_GAME_SCREEN_HPP
#define END_GAME_SCREEN_HPP

#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>
#include "engine/Engine.hpp"
#include "engine/AudioPlayer.hpp"

class EndGameScreen : public GameScreen
{
public:
    void init() override;
    void onUpdate(float dt) override;
    void onGamepadEvent(GamepadEvent gpe);
    void onDraw(sf::RenderTarget& ctx, sf::RenderStates states) const override;
    static const char* destinationForButton(const std::string& button);

protected:
    sf::Text text;
    sf::Text summary_text;
    sf::Text press_any_button;
    sf::Clock clock;

    bool showing = false;
    bool can_leave = false;
    float delay = 3; // seconds before the player can leave

    hh::Sound over;
    float trans = 255.f;
    sf::RectangleShape blackness;
};

#endif
