#ifndef GAME_STORY_SCREEN_HPP
#define GAME_STORY_SCREEN_HPP

#include <SFML/Graphics.hpp>
#include <memory>
#include "engine/Engine.hpp"
#include "engine/ResourceManager.hpp"


class GamestoryScreen : public GameScreen
{
public:
    void init() override;
    void onDraw(sf::RenderTarget& ctx, sf::RenderStates states) const override;
    void onUpdate(float dt) override;

protected:
    void onGamepadEvent(GamepadEvent e);
    // NOTE: this screen used to own an sf::Music member, but its
    // implementation never actually loaded or played anything. The
    // field has been removed along with the SFML/Audio include now
    // that engine audio routes through hh::AudioPlayer.
    bool changed = false;
    sf::Sprite sprite;
    sf::RectangleShape blackness;
    float trans = 255.f;
};

#endif
