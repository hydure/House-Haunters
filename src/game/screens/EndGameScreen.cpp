#include "game/screens/EndGameScreen.hpp"
#include "engine/ModConfig.hpp"
#include "engine/Paths.hpp"
#include "engine/Constants.hpp"

void EndGameScreen::init()
{
    clock.restart();
    trans = 255.f;
    this->showing = false;
    this->can_leave = false;

    // "Game Over" header.
    text.setString("Game Over");
    text.setFont(*ResourceManager::getFont(Paths::resource("fonts/youmurderer.ttf")));
    text.setCharacterSize(100);
    sf::FloatRect pos = text.getLocalBounds();
    text.setPosition(720 / 2, 480 / 2);
    text.setOrigin(pos.left + pos.width / 2, pos.top + pos.height / 2);

    // "Press Any Button" prompt sits just below the header.
    press_any_button.setString("Press Any Button");
    press_any_button.setFont(*ResourceManager::getFont(Paths::resource("fonts/youmurderer.ttf")));
    sf::FloatRect pos2 = press_any_button.getLocalBounds();
    press_any_button.setPosition(720 / 2, 480 / 2 + pos.top + pos.height + 20);
    press_any_button.setOrigin(pos2.left + pos2.width / 2, pos2.top + pos2.height / 2);

    // RAII subscription: released by the engine when this screen exits.
    this->subscribe("gamepad_event", [this](base_event_type e) {
        auto gpe = dynamic_cast< GamepadEvent& >(*e);
        this->onGamepadEvent(gpe);
    });

    over.load(Paths::resource(ModConfig::instance().audio().endgame_music));
    over.play();

    blackness.setSize(sf::Vector2f(720, 480));
    blackness.setFillColor(sf::Color(0, 0, 0, static_cast<sf::Uint8>(trans)));
}

void EndGameScreen::onUpdate(float dt)
{
    if (trans > 50.f) {
        trans -= EngineConstants::kFadeRate * dt;
        if (trans < 50.f) trans = 50.f;
        blackness.setFillColor(sf::Color(0, 0, 0, static_cast<sf::Uint8>(trans)));
    }
    if (!showing && clock.getElapsedTime().asSeconds() >= delay) {
        showing = true;
        can_leave = true;
    }
}

void EndGameScreen::onGamepadEvent(GamepadEvent gpe)
{
    if (gpe.type != GamepadEvent::RELEASED) return;
    if (!can_leave) return;
    config->player_map.clear();
    config->char_map.clear();
    config->num_players = 1;
    auto event = std::make_shared< Event<std::string> >("Title");
    Events::triggerEvent("change_screen", event);
}

void EndGameScreen::onDraw(sf::RenderTarget& ctx, sf::RenderStates /*states*/) const
{
    ctx.setView(ctx.getDefaultView());
    ctx.draw(text);
    ctx.draw(blackness);
    if (showing) {
        ctx.draw(press_any_button);
    }
}
