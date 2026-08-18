#include "game/screens/EndGameScreen.hpp"
#include "engine/ModConfig.hpp"
#include "engine/Paths.hpp"
#include "engine/Constants.hpp"
#include "game/RunSummary.hpp"

void EndGameScreen::init()
{
    clock.restart();
    trans = 255.f;
    this->showing = false;
    this->can_leave = false;

    const auto& summary = RunSummary::instance();

    text.setString(summary.won() ? "YOU ESCAPED" : "THE HOUSE WON");
    text.setFont(*ResourceManager::getFont(Paths::resource("fonts/youmurderer.ttf")));
    text.setCharacterSize(72);
    sf::FloatRect pos = text.getLocalBounds();
    text.setPosition(720 / 2, 105);
    text.setOrigin(pos.left + pos.width / 2, pos.top + pos.height / 2);

    summary_text.setFont(*ResourceManager::getFont(Paths::resource("fonts/Underdog-Regular.ttf")));
    summary_text.setCharacterSize(21);
    summary_text.setFillColor(sf::Color(235, 225, 190));
    summary_text.setStyle(sf::Text::Bold);
    summary_text.setString(
        "TIME  " + RunSummary::formatDuration(summary.elapsedSeconds())
        + "     DIFFICULTY  " + Config::difficultyName(summary.difficulty())
        + "\nEVIDENCE  " + std::to_string(summary.evidenceFound())
        + "     ROOMS  " + std::to_string(summary.rooms())
        + "\nDAMAGE  " + std::to_string(summary.damageDealt())
        + "     FALLEN  " + std::to_string(summary.deaths())
        + "/" + std::to_string(summary.players()));
    sf::FloatRect summaryBounds = summary_text.getLocalBounds();
    summary_text.setOrigin(summaryBounds.left + summaryBounds.width / 2.f, 0.f);
    summary_text.setPosition(720.f / 2.f, 185.f);

    press_any_button.setString("A / Z  REMATCH       B / MENU  TITLE");
    press_any_button.setFont(*ResourceManager::getFont(Paths::resource("fonts/youmurderer.ttf")));
    press_any_button.setCharacterSize(28);
    sf::FloatRect pos2 = press_any_button.getLocalBounds();
    press_any_button.setPosition(720 / 2, 390);
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
    const char* destination = destinationForButton(gpe.button);
    if (destination[0] == '\0') return;
    if (std::string(destination) == "Title") {
        config->player_map.clear();
        config->char_map.clear();
        config->num_players = 1;
    }
    auto event = std::make_shared< Event<std::string> >(destination);
    Events::triggerEvent("change_screen", event);
}

const char* EndGameScreen::destinationForButton(const std::string& button)
{
    if (button == "A" || button == "START") return "GamePlay";
    if (button == "B" || button == "MENU") return "Title";
    return "";
}

void EndGameScreen::onDraw(sf::RenderTarget& ctx, sf::RenderStates /*states*/) const
{
    ctx.setView(ctx.getDefaultView());
    ctx.draw(text);
    ctx.draw(summary_text);
    ctx.draw(blackness);
    if (showing) {
        ctx.draw(press_any_button);
    }
}
