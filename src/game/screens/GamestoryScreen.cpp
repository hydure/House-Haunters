#include "game/screens/GamestoryScreen.hpp"
#include "engine/Paths.hpp"
#include "engine/Constants.hpp"

void GamestoryScreen::init()
{
    // RAII subscription: released by the engine when this screen exits.
    this->subscribe("gamepad_event", [this](base_event_type e) {
        auto gpe = dynamic_cast< GamepadEvent& >(*e);
        this->onGamepadEvent(gpe);
    });

    sprite.setTexture(*ResourceManager::getTexture(Paths::resource("storyscreen.png")));

    blackness.setSize(sf::Vector2f(720, 480));
    blackness.setFillColor(sf::Color(0, 0, 0, static_cast<sf::Uint8>(trans)));
}

void GamestoryScreen::onUpdate(float dt)
{
    // Screen emerges from darkness at ~60 alpha units / sec.
    if (trans > 50.f) {
        trans -= EngineConstants::kFadeRate * dt;
        if (trans < 50.f) trans = 50.f;
        blackness.setFillColor(sf::Color(0, 0, 0, static_cast<sf::Uint8>(trans)));
    }
}

void GamestoryScreen::onGamepadEvent(GamepadEvent /*e*/)
{
    auto event = std::make_shared< Event<std::string> >("Title");
    this->changed = true;
    Events::triggerEvent("change_screen", event);
}

void GamestoryScreen::onDraw(sf::RenderTarget& ctx, sf::RenderStates /*states*/) const
{
    ctx.draw(sprite);
    ctx.draw(blackness);
}
