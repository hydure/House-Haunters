#include "game/screens/CharacterScreen.hpp"
#include "engine/Paths.hpp"
#include "engine/Constants.hpp"
#include "engine/ModConfig.hpp"

void CharacterIcon::onDraw(sf::RenderTarget& ctx, sf::RenderStates states) const
{
    ctx.draw(t, states);
}

std::vector<std::unique_ptr<CharacterIcon>>::iterator CharacterSelection::find(int player)
{
    return std::find_if(hovering.begin(), hovering.end(),
        [=](std::unique_ptr<CharacterIcon>& c) { return c->getPlayer() == player; });
}

void CharacterSelection::setPortrait(int idx)
{
    this->index = idx;
    const auto& sm = ModConfig::instance().screen();
    portrait.setTexture(*ResourceManager::getTexture(Paths::resource(sm.portrait_atlas)));
    // Scale is mod-driven; default 0.586 ~= 0.5 * (300/256) preserves the
    // historic on-screen size after the source atlas was downsized from
    // 1200x900 to 1024x768 to fit under the 1024 GDI Generic cap.
    portrait.setScale(sm.portrait_scale, sm.portrait_scale);
    // Fake an outline by drawing a slightly larger background rectangle behind the
    // portrait (sf::Sprite has no outline support in SFML <= 2.4).
    const int padding = 3;
    const float displayedW = sm.portrait_tile_w * sm.portrait_scale;
    const float displayedH = sm.portrait_tile_h * sm.portrait_scale;
    background.setSize(sf::Vector2f(displayedW + 2 * padding, displayedH + 2 * padding));
    background.setPosition(-padding, -padding);
    unsetPlayer();
}

void CharacterSelection::setPlayer(int player)
{
    player_selected = player;
    selected = true;
    const auto& sm = ModConfig::instance().screen();
    const int cols = sm.portrait_atlas_cols > 0 ? sm.portrait_atlas_cols : 1;
    const int x = index % cols;
    const int y = index / cols;
    portrait.setTextureRect(sf::IntRect(x * sm.portrait_tile_w, y * sm.portrait_tile_h,
                                        sm.portrait_tile_w, sm.portrait_tile_h));
    background.setFillColor(colors[(player - 1) % 4]);
}

void CharacterSelection::unsetPlayer()
{
    player_selected = -1;
    selected = false;
    const auto& sm = ModConfig::instance().screen();
    const int cols = sm.portrait_atlas_cols > 0 ? sm.portrait_atlas_cols : 1;
    const int x = (index + 1) % cols;
    const int y = (index + 1) / cols;
    background.setFillColor(sf::Color::White);
    portrait.setTextureRect(sf::IntRect(x * sm.portrait_tile_w, y * sm.portrait_tile_h,
                                        sm.portrait_tile_w, sm.portrait_tile_h));
}

void CharacterSelection::removePlayer(int player)
{
    auto it = find(player);
    if (it != hovering.end()) {
        hovering.erase(it);
    }
}

void CharacterSelection::addPlayer(int player)
{
    if (find(player) != hovering.end()) {
        return;
    }
    auto c = std::make_unique<CharacterIcon>();
    c->setPlayer(player);
    c->setFont(Paths::resource("fonts/Underdog-Regular.ttf"));
    c->setColor(colors[(player - 1) % 4]);
    hovering.push_back(std::move(c));
}

bool CharacterSelection::hasPlayer(int player)
{
    return find(player) != hovering.end();
}

void CharacterSelection::onUpdate(float /*dt*/)
{
    const float width    = 150; // arbitrary box width for now
    const float box_size = 30;
    const float padding  = 5;
    const float xpos     = (width / 2) - (box_size / 2);
    for (const auto& icon : hovering) {
        const int player = icon->getPlayer();
        const float ypos = 225 + (player - 1) * box_size + player * padding;
        icon->setPosition(xpos, ypos);
    }
}

void CharacterSelection::onDraw(sf::RenderTarget& ctx, sf::RenderStates states) const
{
    for (const auto& icon : hovering) {
        ctx.draw(*icon, states);
    }
    ctx.draw(background, states);
    ctx.draw(portrait, states);
}


void CharacterScreen::init()
{
    this->char_selections.clear();
    this->selected_count = 0;
    this->trans = 255.f;

    chara_sound.load(Paths::resource(ModConfig::instance().audio().lobby_select_sfx));

    player_num = static_cast<int>(config->player_map.size());

    // When entered with no players yet in player_map, hold a "PRESS ANY
    // BUTTON" gate so somebody can plug in more controllers before
    // anyone is locked into a slot. The first RELEASED gamepad event
    // (any controller, any button) registers that input source as P1
    // and drops the gate. While the gate is up, controllers that hot
    // plug in are silently registered as P2/P3/P4 so they appear on
    // the screen the moment they're recognized.
    awaiting_p1 = (player_num == 0);

    // ModConfig drives the lobby's layout, title, color, and per-character
    // portrait choice. Defaults (see ModConfig::applyDefaults) match the
    // historic hardcoded values so an unmodded launch is unchanged.
    const auto& mc = ModConfig::instance();
    const auto& sm = mc.screen();

    // Initialize the four character portraits.
    float xpos = sm.layout_start_x;
    for (int i = 0; i < 4; i++) {
        auto c = std::make_unique<CharacterSelection>();
        c->setCharacter(static_cast<Config::CHARACTER>(i));
        c->setPortrait(mc.character(i).portrait_index);
        c->setPosition(xpos, sm.layout_start_y);
        this->char_selections.push_back(std::move(c));
        xpos += sm.layout_x_spacing;
    }

    // Seed any players who were already in the map (legacy path from
    // GametitleScreen's confirmMenuSelection, and the same path our
    // CTest screen-flow tests drive).
    for (const auto& kv : config->player_map) {
        this->addPlayer(kv.first, kv.second);
    }

    // RAII subscriptions: released by the engine when this screen exits.
    this->subscribe("gamepad_event", [this](base_event_type e) {
        auto gpe = dynamic_cast< GamepadEvent& >(*e);
        this->onGamepadEvent(gpe);
    });
    // Hot-plug bridge: any controller that appears mid-lobby gets the
    // next available player slot (up to four). We do NOT wait for the
    // newcomer to press a button -- the slot appears immediately so
    // it's obvious to the host that the controller was recognized.
    this->subscribe("gamepad_connect", [this](base_event_type e) {
        auto gpe = dynamic_cast< GamepadEvent& >(*e);
        this->onGamepadConnect(gpe);
    });

    background.setSize(sf::Vector2f(720, 480));
    background.setFillColor(sm.background_color);

    teamFont.setFont(*ResourceManager::getFont(Paths::resource(sm.title_font)));
    teamFont.setString(awaiting_p1
        ? "PLUG IN CONTROLLERS - PRESS ANY BUTTON TO BEGIN"
        : sm.title_text);
    teamFont.setCharacterSize(static_cast<unsigned int>(sm.title_size));
    teamFont.setStyle(sf::Text::Bold);
    teamFont.setPosition(static_cast<float>(sm.title_x), static_cast<float>(sm.title_y));

    blackness.setSize(sf::Vector2f(720, 480));
    blackness.setFillColor(sf::Color(0, 0, 0, static_cast<sf::Uint8>(trans)));
}

void CharacterScreen::onUpdate(float dt)
{
    // Title screen emerges from darkness.
    // ~60 alpha units / second matches the original (frame-rate-coupled) feel
    // without depending on the engine running at 60 fps.
    if (trans > 50.f) {
        trans -= EngineConstants::kFadeRate * dt;
        if (trans < 50.f) trans = 50.f;
        blackness.setFillColor(sf::Color(0, 0, 0, static_cast<sf::Uint8>(trans)));
    }
    for (const auto& c : char_selections) {
        c->update(dt);
    }
}

void CharacterScreen::addPlayer(int index, int num)
{
    config->player_map[index] = num;
    // Assign this player to the first character slot that's free.
    for (const auto& c : char_selections) {
        if (!c->isSelected() && c->isEmpty()) {
            c->addPlayer(num);
            return;
        }
    }
}

void CharacterScreen::onGamepadConnect(GamepadEvent e)
{
    // Skip while the press-any-button gate is up; the gate handler
    // wants the first input itself so the host -- not whoever happened
    // to plug in last -- gets the P1 slot. Hot-plugged pads are
    // still recognized by the hardware layer and can press a button
    // to be the one that drops the gate.
    if (awaiting_p1) {
        return;
    }
    // Don't double-add: a controller that's already registered to a
    // player stays put.
    if (config->player_map.count(e.index) != 0) {
        return;
    }
    // Cap at four. After that, additional pads sit idle until a slot
    // opens up (which today only happens on lobby restart).
    if (player_num >= 4) {
        return;
    }
    player_num++;
    this->addPlayer(e.index, player_num);
    teamFont.setString("MAKE YOUR TEAM");
}

void CharacterScreen::onGamepadEvent(GamepadEvent e)
{
    if (e.type != GamepadEvent::RELEASED) {
        return;
    }
    // "Press any button" gate. First release of any button on any
    // controller drops the gate and registers that input source as P1.
    // Returning here means the gate-lifting press does NOT also count
    // as a character pick or a back-out -- the user only wanted to
    // signal that the lobby may begin.
    if (awaiting_p1) {
        // MENU on the gate-screen still routes back to the title in
        // case the host changes their mind before anyone has joined.
        if (e.button == "MENU") {
            auto event = std::make_shared< Event<std::string> >("Title");
            Events::triggerEvent("change_screen", event);
            return;
        }
        awaiting_p1 = false;
        player_num  = 1;
        this->addPlayer(e.index, 1);
        const auto& sm = ModConfig::instance().screen();
        teamFont.setString(sm.title_text);
        return;
    }
    // MENU (Escape on keyboard, Start on gamepad) backs out to the title
    // screen. Handled BEFORE the new-player branch so a wandering player
    // can't be "joined" by mistake when they only meant to back out.
    if (e.button == "MENU") {
        auto event = std::make_shared< Event<std::string> >("Title");
        Events::triggerEvent("change_screen", event);
        return;
    }
    // New player joining the lobby.
    if (player_num < 4 && config->player_map.count(e.index) == 0) {
        player_num++;
        this->addPlayer(e.index, player_num);
        teamFont.setString("MAKE YOUR TEAM");
        return;
    }

    const int player = config->player_map[e.index];
    int found_index = -1;
    int replace_index = -1;
    bool found = false;

    if (e.button == "RIGHT") {
        int index = 0;
        for (const auto& c : char_selections) {
            if (!found && c->hasPlayer(player)) {
                if (c->isSelected() && c->getPlayer() == player) break;
                found_index = index;
                found = true;
            }
            else if (found) {
                // Skip characters already locked in by another player so
                // each player ends up with a unique choice.
                if (c->isSelected()) { index++; continue; }
                replace_index = index;
                break;
            }
            index++;
        }
    }
    else if (e.button == "LEFT") {
        int index = static_cast<int>(char_selections.size()) - 1;
        for (auto it = char_selections.rbegin(); it != char_selections.rend(); ++it) {
            if (!found && (*it)->hasPlayer(player)) {
                if ((*it)->isSelected() && (*it)->getPlayer() == player) break;
                found_index = index;
                found = true;
            }
            else if (found) {
                // Skip characters already locked in by another player so
                // each player ends up with a unique choice.
                if ((*it)->isSelected()) { index--; continue; }
                replace_index = index;
                break;
            }
            index--;
        }
    }
    else if (e.button == "A" || e.button == "START") {
        chara_sound.play();
        if (selected_count == player_num) {
            auto event = std::make_shared< Event<std::string> >("GamePlay");
            this->changed = true;

            // Push the chosen team into the shared config.
            config->num_players = selected_count;
            for (const auto& c : char_selections) {
                if (c->isSelected()) {
                    const int p = c->getPlayer();
                    config->char_map[p] = c->getCharacter();
                }
            }
            Events::triggerEvent("change_screen", event);
            return;
        }
        // Lock in the character this player is currently hovering.
        for (const auto& c : char_selections) {
            if (c->isSelected()) continue;
            if (c->hasPlayer(player)) {
                c->setPlayer(player);
                if (++selected_count == player_num) {
                    teamFont.setString("PRESS A/Z TO START");
                }
                break;
            }
        }
    }
    else if (e.button == "B") {
        for (const auto& c : char_selections) {
            if (c->isSelected() && c->hasPlayer(player)) {
                c->unsetPlayer();
                selected_count--;
                teamFont.setString("MAKE YOUR TEAM");
                break;
            }
        }
    }

    // Move this player's hover one slot over.
    if (found_index >= 0 && found_index < 4 && replace_index >= 0 && replace_index < 4) {
        char_selections[found_index]->removePlayer(player);
        char_selections[replace_index]->addPlayer(player);
    }
}

void CharacterScreen::onDraw(sf::RenderTarget& ctx, sf::RenderStates /*states*/) const
{
    ctx.draw(background);
    ctx.draw(teamFont);
    for (const auto& c : char_selections) {
        ctx.draw(*c);
    }
    ctx.draw(blackness);
}
