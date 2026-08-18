#include "game/screens/GametitleScreen.hpp"
#include "engine/ModConfig.hpp"
#include "engine/Paths.hpp"
#include "engine/Constants.hpp"

#include <cmath>

void GametitleScreen::init()
{
    // Stop any previous playback so re-init (e.g. returning here after
    // canceling out of the Host/Join screens) releases the file handle
    // before we try to reopen it. Reopening a still-playing sf::Music
    // returns false on some SFML builds, which used to early-out of this
    // entire function and leave the title screen blank.
    music.stop();
    if (music.openFromFile(Paths::resource(ModConfig::instance().audio().title_music))) {
        music.play();
        music.setLoop(true);
    }
    // NOTE: missing music is non-fatal -- the rest of the title screen
    // (logo, menu, prompts) is still set up below so the user always has
    // something to look at and a working menu.

    // RAII subscription: released when the engine leaves this screen.
    this->subscribe("gamepad_event", [this](base_event_type e) {
        auto gpe = dynamic_cast< GamepadEvent& >(*e);
        this->onGamepadEvent(gpe);
    });

    sprite.setTexture(*ResourceManager::getTexture(Paths::resource("titlescreen.png")));

    blackness.setSize(sf::Vector2f(720, 480));
    blackness.setFillColor(sf::Color(0, 0, 0, static_cast<sf::Uint8>(trans)));

    // IMPORTANT: do NOT reset `pressed` here. The member is constructed
    // to false (default initializer in the header) so the very first
    // entry shows the "press any button" gate; every subsequent entry
    // -- e.g. backing out of Host/Join/Character -- finds `pressed`
    // already true and goes straight to the menu. Resetting it forced
    // the user to press an extra button on every return trip.
    difficulty_text.setFont(*ResourceManager::getFont(Paths::resource("fonts/Underdog-Regular.ttf")));
    difficulty_text.setCharacterSize(20);
    difficulty_text.setFillColor(sf::Color::White);
    difficulty_text.setStyle(sf::Text::Bold);

    difficulty_detail.setFont(*ResourceManager::getFont(Paths::resource("fonts/Underdog-Regular.ttf")));
    difficulty_detail.setCharacterSize(13);
    difficulty_detail.setFillColor(sf::Color(235, 225, 190));
    updateDifficultyText();

    // Hint about the controls screen. Always visible (no "first press"
    // gate) so brand-new players see how to look up the controls.
    controls_hint.setFont(*ResourceManager::getFont(Paths::resource("fonts/Underdog-Regular.ttf")));
    controls_hint.setCharacterSize(16);
    controls_hint.setFillColor(sf::Color(220, 220, 220));
    controls_hint.setStyle(sf::Text::Italic);
    controls_hint.setString("Press Y for CONTROLS");
    {
        sf::FloatRect b = controls_hint.getLocalBounds();
        controls_hint.setOrigin(b.left + b.width / 2.f, 0.f);
        controls_hint.setPosition(720.f / 2.f, 480.f - 24.f);
    }

    // Primary call-to-action. Now sits in the dark sky band ABOVE the
    // title art so it never overlaps the logo scratches. Always visible
    // so the player knows the screen isn't stuck. Label depends on
    // whether the player has already passed the press-to-continue gate;
    // re-entering the title from a sub-menu shouldn't show the gate
    // message again.
    continue_prompt.setFont(*ResourceManager::getFont(Paths::resource("fonts/Underdog-Regular.ttf")));
    continue_prompt.setCharacterSize(20);
    continue_prompt.setFillColor(sf::Color::White);
    continue_prompt.setStyle(sf::Text::Bold);
    continue_prompt.setString(pressed
        ? "UP/DOWN to choose   A/Z to start"
        : "PRESS ANY BUTTON TO CONTINUE");
    {
        sf::FloatRect b = continue_prompt.getLocalBounds();
        continue_prompt.setOrigin(b.left + b.width / 2.f, 0.f);
        continue_prompt.setPosition(720.f / 2.f, 30.f);
    }
    prompt_time = 0.f;

    // Build the main menu (hidden until first press). Three items, vertical
    // list, placed BELOW the logo scratches so they don't overlap the
    // title art. UP/DOWN navigates; A/START confirms; LEFT/RIGHT still
    // cycles difficulty (visible right below the menu).
    menu_lines.clear();
    // Keep the previously highlighted menu item across re-entry so
    // backing out preserves the user's place; only initialize on the
    // very first construction (cursor starts at 0 via default).
    const std::vector<std::string> items = {
        "LOCAL PLAY",
        "HOST GAME",
        "JOIN GAME",
    };
    const float menuTopY  = 480.f - 130.f; // first line baseline
    const float menuStep  = 26.f;
    for (size_t i = 0; i < items.size(); ++i) {
        sf::Text t;
        t.setFont(*ResourceManager::getFont(Paths::resource("fonts/Underdog-Regular.ttf")));
        t.setCharacterSize(22);
        t.setStyle(sf::Text::Bold);
        t.setString(items[i]);
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(b.left + b.width / 2.f, 0.f);
        t.setPosition(720.f / 2.f, menuTopY + menuStep * static_cast<float>(i));
        menu_lines.push_back(std::move(t));
    }
    // Dark backdrop behind the menu so the white / yellow text reads
    // clearly against the logo. Sized to wrap all three items + 6px
    // padding on every side.
    const float menuPad = 8.f;
    const float menuW   = 280.f;
    const float menuH   = menuStep * static_cast<float>(items.size()) + menuPad * 2.f;
    menu_backdrop.setSize(sf::Vector2f(menuW, menuH));
    menu_backdrop.setFillColor(sf::Color(0, 0, 0, 170));
    menu_backdrop.setOutlineThickness(1.f);
    menu_backdrop.setOutlineColor(sf::Color(255, 220, 80, 120));
    menu_backdrop.setPosition((720.f - menuW) / 2.f, menuTopY - menuPad);

    updateMenuText();
}

void GametitleScreen::updateMenuText()
{
    // Highlight the active item in yellow; dim the others.
    const float menuTopY = 480.f - 130.f;
    const float menuStep = 26.f;
    for (size_t i = 0; i < menu_lines.size(); ++i) {
        if (static_cast<int>(i) == menu_cursor) {
            menu_lines[i].setFillColor(sf::Color(255, 220, 80));
            menu_lines[i].setString(std::string("> ") + (
                i == 0 ? "LOCAL PLAY"
              : i == 1 ? "HOST GAME"
              :          "JOIN GAME"
            ) + " <");
        }
        else {
            menu_lines[i].setFillColor(sf::Color(200, 200, 200));
            menu_lines[i].setString(
                i == 0 ? "LOCAL PLAY"
              : i == 1 ? "HOST GAME"
              :          "JOIN GAME"
            );
        }
        sf::FloatRect b = menu_lines[i].getLocalBounds();
        menu_lines[i].setOrigin(b.left + b.width / 2.f, 0.f);
        menu_lines[i].setPosition(720.f / 2.f, menuTopY + menuStep * static_cast<float>(i));
    }
}

void GametitleScreen::confirmMenuSelection()
{
    const char* target = "Character"; // LOCAL PLAY default
    if (menu_cursor == 1)      target = "Host";
    else if (menu_cursor == 2) target = "Join";

    // LOCAL PLAY no longer pre-seeds player_map: CharacterScreen now
    // opens an "awaiting P1" gate so a host can plug in extra
    // controllers before anyone is locked into a slot. The first
    // button release on any controller drops the gate and claims P1.
    if (menu_cursor == 0) {
        this->config->player_map.clear();
        this->config->char_map.clear();
        this->config->num_players = 0;
    }
    auto evt = std::make_shared< Event<std::string> >(target);
    Events::triggerEvent("change_screen", evt);
}

void GametitleScreen::updateDifficultyText()
{
    std::string label = std::string("DIFFICULTY: ")
                      + Config::difficultyName(config->difficulty)
                      + "  (LEFT/RIGHT to change)";
    difficulty_text.setString(label);
    sf::FloatRect b = difficulty_text.getLocalBounds();
    difficulty_text.setOrigin(b.left + b.width / 2.f, 0.f);
    difficulty_text.setPosition(720.f / 2.f, 480.f - 50.f);

    difficulty_detail.setString(std::string(Config::difficultyDescription(config->difficulty))
        + " | HAUNT rises 90s / 180s after arrival");
    sf::FloatRect detailBounds = difficulty_detail.getLocalBounds();
    difficulty_detail.setOrigin(detailBounds.left + detailBounds.width / 2.f, 0.f);
    difficulty_detail.setPosition(720.f / 2.f, 480.f - 168.f);
}

void GametitleScreen::onUpdate(float dt)
{
    // Title screen emerges from darkness at ~60 alpha units / sec.
    if (trans > 50.f) {
        trans -= EngineConstants::kFadeRate * dt;
        if (trans < 50.f) trans = 50.f;
        blackness.setFillColor(sf::Color(0, 0, 0, static_cast<sf::Uint8>(trans)));
    }
    // Pulse the continue prompt between ~120 and 255 alpha at ~1Hz so it
    // reads as "waiting for input" instead of dead UI text.
    prompt_time += dt;
    const float pulse = 0.5f * (1.f + std::sin(prompt_time * 3.14159f));
    const sf::Uint8 alpha = static_cast<sf::Uint8>(135.f + 120.f * pulse);
    sf::Color c = continue_prompt.getFillColor();
    c.a = alpha;
    continue_prompt.setFillColor(c);
}

void GametitleScreen::onGamepadEvent(GamepadEvent e)
{
    // Cycle / advance only on key release for predictable repeats.
    if (e.type != GamepadEvent::RELEASED) {
        return;
    }
    // Y opens the controls reference screen at any time -- even before the
    // first acknowledgement press -- so brand-new players can look up the
    // bindings before doing anything else.
    if (e.button == "Y") {
        auto event = std::make_shared< Event<std::string> >("Controls");
        Events::triggerEvent("change_screen", event);
        return;
    }
    // First press just acknowledges the title screen; subsequent presses can
    // either tweak difficulty (LEFT/RIGHT), navigate the menu (UP/DOWN), or
    // confirm the highlighted option (A/START).
    if (!this->pressed) {
        this->pressed = true;
        updateDifficultyText();
        updateMenuText();
        // The menu now drives advancement -- swap the prompt so the player
        // knows the next press starts whatever option they pick. Keep it
        // in the upper "sky" band so it doesn't crowd the logo or menu.
        continue_prompt.setString("UP/DOWN to choose   A/Z to start");
        sf::FloatRect b = continue_prompt.getLocalBounds();
        continue_prompt.setOrigin(b.left + b.width / 2.f, 0.f);
        continue_prompt.setPosition(720.f / 2.f, 30.f);
        return;
    }
    if (e.button == "LEFT") {
        int next = (static_cast<int>(config->difficulty) + 2) % 3; // wrap backward
        config->difficulty = static_cast<Config::DIFFICULTY>(next);
        updateDifficultyText();
        return;
    }
    if (e.button == "RIGHT") {
        int next = (static_cast<int>(config->difficulty) + 1) % 3; // wrap forward
        config->difficulty = static_cast<Config::DIFFICULTY>(next);
        updateDifficultyText();
        return;
    }
    if (e.button == "UP") {
        menu_cursor = (menu_cursor + static_cast<int>(menu_lines.size()) - 1)
                    % static_cast<int>(menu_lines.size());
        updateMenuText();
        return;
    }
    if (e.button == "DOWN") {
        menu_cursor = (menu_cursor + 1) % static_cast<int>(menu_lines.size());
        updateMenuText();
        return;
    }
    if (e.button == "A" || e.button == "START") {
        // Confirm the highlighted menu option (LOCAL/HOST/JOIN).
        if (menu_cursor == 0) {
            this->config->player_map[e.index] = 1;
        }
        confirmMenuSelection();
        return;
    }
    // Any other button is ignored once the menu is visible -- we don't want
    // a stray B/X press to accidentally launch the wrong game mode.
}

void GametitleScreen::onDraw(sf::RenderTarget& ctx, sf::RenderStates /*states*/) const
{
    ctx.draw(sprite);
    if (pressed) {
        ctx.draw(menu_backdrop);
        ctx.draw(difficulty_detail);
        ctx.draw(difficulty_text);
        for (const auto& l : menu_lines) {
            ctx.draw(l);
        }
    }
    ctx.draw(continue_prompt);
    ctx.draw(controls_hint);
    ctx.draw(blackness);
}
