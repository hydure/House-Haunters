#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include "game/characters/PlayerView.hpp"
#include "game/screens/PauseMenu.hpp"
#include "engine/Paths.hpp"
#include "engine/ResourceFS.hpp"

void PlayerView::init()
{
    shaderReady = false;
    if (!sf::Shader::isAvailable()) {
        // No GLSL support on this OpenGL context (e.g. Windows fell back
        // to GDI Generic software rendering). Skip the shader entirely;
        // onDraw will render the lighting rect with no shader bound.
        // The default fill color of sf::RectangleShape is opaque white,
        // so we MUST explicitly clear it to transparent or the unshaded
        // lighting rect paints the whole HUD white every frame (the
        // "white background, hearts only" symptom).
        lighting.setFillColor(sf::Color::Transparent);
        // Static guard so a 4-player game only logs this once instead
        // of four times (PlayerView::init runs per player).
        static bool warned = false;
        if (!warned) {
            warned = true;
            std::cout << "Shaders are not available on this machine; "
                         "HUD vignette disabled." << std::endl;
        }
    }
    else {
        lighting.setFillColor(sf::Color(0, 0, 0, 0));
        // sf::Shader::loadFromMemory wants string sources, not byte
        // buffers (GLSL is just text). Route through ResourceFS so the
        // same call site works in both build modes: disk in development,
        // embedded RT_RCDATA in the standalone build. We use readAll
        // because the shader source isn't kept alive after compile, so
        // a transient std::vector<char> is the cheapest option.
        auto loadShaderSource = [](const std::string& path, std::string& out) -> bool {
            std::vector<char> bytes;
            if (!hh::ResourceFS::readAll(path, bytes)) return false;
            out.assign(bytes.begin(), bytes.end());
            return true;
        };
        std::string vertSrc, fragSrc;
        const bool vertOk = loadShaderSource(Paths::resource("shaders/VertexShader.txt"), vertSrc);
        const bool fragOk = loadShaderSource(Paths::resource("shaders/GradientShader.txt"), fragSrc);
        if (vertOk && fragOk &&
            shader.loadFromMemory(vertSrc, fragSrc)) {
            shaderReady = true;
            lighting.setFillColor(sf::Color::Black);
            shader.setUniform("windowHeight", lighting.getSize().y);
            // Lighting center varies by player layout (3+ players use top half).
            if (numPlayers >= 3) {
                shader.setUniform("center", sf::Vector2f(
                    viewport_x + lighting.getSize().x / 2.0f,
                    viewport_y - lighting.getSize().y / 2.0f));
            }
            else {
                shader.setUniform("center", sf::Vector2f(
                    viewport_x + lighting.getSize().x / 2.0f,
                    lighting.getSize().y / 2.0f));
            }
            shader.setUniform("radius", std::min(lighting.getSize().y, lighting.getSize().x) / 2.2f);
            shader.setUniform("expand", 0.0f);
        }
        else {
            // Likewise dedupe across players.
            static bool loadWarned = false;
            if (!loadWarned) {
                loadWarned = true;
                std::cout << "Failed to load lighting shader; HUD vignette disabled."
                          << std::endl;
            }
        }
    }

    itemBar.setSize(sf::Vector2f(20, 20));
    itemBar.setOutlineColor(sf::Color::White);
    itemBar.setFillColor(sf::Color::Transparent);
    itemBar.setOutlineThickness(2);
    painCount = 100;
    pain.setFillColor(sf::Color(255, 0, 0, static_cast<sf::Uint8>(painCount)));

    heartTexture = *ResourceManager::getTexture(Paths::resource("sprites/heart.png"));

    // One shared heart sprite is reused for every heart we draw -- just
    // re-position + re-tex-rect it in onDraw instead of constructing a new
    // sf::Sprite per heart per frame.
    heartSprite.setTexture(heartTexture);
    heartSprite.setScale(sf::Vector2f(0.1f, 0.1f));

    // Forward gamepad input for our player to that player's Character.
    // Held in an EventSubscription member so the listener auto-detaches when
    // this PlayerView is destroyed (e.g. when GameplayScreen::onExit fires).
    gamepadSub = EventSubscription(
        Events::addEventListener("gamepad_event", [this](base_event_type e) {
            // While the in-game pause menu is open, swallow gameplay
            // input so the character doesn't keep walking / attacking
            // behind the overlay. GameplayScreen zeroes character
            // direction on pause-open so a previously held key doesn't
            // get stuck on resume.
            if (PauseMenu::inputBlocked()) {
                return;
            }
            if (!entity_group) {
                return;
            }
            auto gpe = dynamic_cast< GamepadEvent& >(*e);
            auto self = entity_group->getCharacter(playernumber);
            if (!self) {
                return;
            }
            if (gpe.index != self->getGamepadIndex()) {
                return;
            }
            // Dead players don't drive a character -- instead they pilot
            // a free-floating spectator camera. LEFT / RIGHT cycle which
            // surviving teammate the camera follows. Any other input is
            // swallowed so a held-down direction from before death
            // doesn't bleed through to the corpse (Character itself
            // already early-outs on health<=0, but the camera target
            // also needs to refresh in case our previous spectator
            // target has since died).
            if (self->health <= 0) {
                ensureSpectator();
                if (gpe.type == GamepadEvent::TYPE::PRESSED) {
                    if (gpe.button == "LEFT") {
                        cycleSpectator(-1);
                    }
                    else if (gpe.button == "RIGHT") {
                        cycleSpectator(+1);
                    }
                }
                return;
            }
            // Alive: drop any leftover spectator override (in case we
            // got revived via a rejoin / HP restore) and forward.
            spectatorTarget_ = -1;
            self->onGamepadEvent(gpe);
        }));
}

void PlayerView::onUpdate(float /*dt*/)
{
    if (!entity_group) {
        return;
    }
    // Re-validate the spectator target every frame so dying mid-game
    // -- or the teammate we were watching ALSO dying -- swaps the
    // camera onto the next survivor without waiting for input.
    ensureSpectator();
    auto target = entity_group->getCharacter(currentTarget());
    if (!target) {
        // No valid target (everyone is dead). Leave the camera frozen
        // at its last position -- the game-end transition is about to
        // fire from GameplayScreen's player_died handler anyway.
        return;
    }
    v.setCenter(target->getPosition());
    bool invul = target->invul;
    if (invul && painCount > 0) {
        pain.setFillColor(sf::Color(255, 0, 0, static_cast<sf::Uint8>(painCount)));
        painCount--;
    }
    else if (!invul) {
        painCount = 100;
    }
}

void PlayerView::setView(sf::FloatRect dimensions, sf::FloatRect viewport)
{
    viewDimensions = dimensions;
    v.reset(dimensions);
    v.setViewport(viewport);
    HUD.reset(dimensions);
    HUD.setViewport(viewport);
    itemBar.setPosition(viewport.left + dimensions.width - 40,
                        viewport.top  + dimensions.height - 40);
    pain.setSize(sf::Vector2f(dimensions.width, dimensions.height));
    lighting.setPosition(0, 0);
    viewport_x = 720 * viewport.left;
    viewport_y = 480 * viewport.top;
    lighting.setSize({dimensions.width, dimensions.height});

    // Clue overlay geometry depends on the viewport size. Set it up once
    // here so onDraw is allocation-free.
    clueBgBox.setSize(sf::Vector2f(dimensions.width - 40, 40));
    clueBgBox.setFillColor(sf::Color::Black);
    clueBgBox.setPosition(20, dimensions.height - 60);

    clueText.setCharacterSize(24);
    clueText.setFillColor(sf::Color::White);
    clueText.setStyle(sf::Text::Bold);
    clueText.setPosition(30, dimensions.height - 55);
    // ResourceManager::getFont is cache-on-first-call and never throws;
    // safe to do here regardless of init() ordering.
    clueText.setFont(*ResourceManager::getFont(Paths::resource("fonts/Underdog-Regular.ttf")));
    clueTextReady = true;
}

void PlayerView::onDraw(sf::RenderTarget& target, sf::RenderStates /*states*/) const
{
    target.setView(v);

    // Spectator-aware: while alive currentTarget() == playernumber, but
    // once we've died the camera + HUD must follow the survivor we're
    // watching instead of staring at our corpse.
    if (!entity_group) {
        return;
    }
    auto follow = entity_group->getCharacter(currentTarget());
    if (!follow) {
        return;
    }

    if (rooms) {
        // Prefer the cached room (updated each tick on the character) over
        // walking the room list every frame; fall back if it's not set yet.
        Room* room = follow->currentRoom
            ? follow->currentRoom
            : rooms->getRoomInside(follow->hbox);
        if (room) {
            target.draw(*room);
            target.draw(*rooms);
            entity_group->drawInArea(target, room->hbox);
        }
    }

    // HUD
    target.setView(HUD);
    // Pass nullptr (no shader) when the shader didn't load -- binding an
    // empty sf::Shader on a context without GLSL support spams stderr
    // every frame with "Failed to bind or unbind shader".
    target.draw(lighting, shaderReady ? sf::RenderStates(&shader) : sf::RenderStates::Default);
    if (follow->invul) {
        target.draw(pain);
    }

    for (int i = 0; i < follow->maxHealth; i++) {
        if (follow->health > i) {
            heartSprite.setTextureRect(sf::IntRect(0,   0, 300, 300));
        }
        else {
            heartSprite.setTextureRect(sf::IntRect(600, 0, 300, 300));
        }
        heartSprite.setPosition(i * 30.f, 0.f);
        target.draw(heartSprite);
    }

    // Clue text box
    auto* character = follow.get();
    if (character->readClue && character->atClue) {
        if (!clueTextReady) {
            clueText.setFont(*ResourceManager::getFont(Paths::resource("fonts/Underdog-Regular.ttf")));
            clueTextReady = true;
        }
        if (character->currentClue != nullptr) {
            clueText.setString(character->currentClue->setClue);
        } else {
            clueText.setString("");
        }

        target.draw(clueBgBox);
        target.draw(clueText);
    }
}

int PlayerView::currentTarget() const
{
    // Once we have a spectator override, prefer it. Validity of the
    // override is the caller's responsibility -- ensureSpectator() runs
    // every frame from onUpdate() and after every gamepad event from
    // the input listener, so by the time onDraw asks for the target
    // it's already pointed at a living teammate (or -1 when nobody is
    // left to spectate, which means the GameEnd screen change is on
    // its way).
    if (spectatorTarget_ > 0) {
        return spectatorTarget_;
    }
    return playernumber;
}

void PlayerView::ensureSpectator()
{
    if (!entity_group) {
        return;
    }
    auto self = entity_group->getCharacter(playernumber);
    // Still alive? Drop any leftover spectator override so the camera
    // snaps back to us (covers the late-rejoin-with-restored-HP case).
    if (self && self->health > 0) {
        spectatorTarget_ = -1;
        return;
    }
    // Dead -- validate the current override.
    if (spectatorTarget_ > 0) {
        auto current = entity_group->getCharacter(spectatorTarget_);
        if (current && !current->isVillain() && current->health > 0) {
            return;
        }
    }
    // Pick the lowest-numbered surviving teammate; deterministic across
    // platforms so a dead player and a spectator-cycle test see the
    // same first target.
    int bestSlot = -1;
    for (const auto& c : entity_group->getCharacters()) {
        if (!c) continue;
        if (c->isVillain()) continue;
        if (c->player_number == playernumber) continue;
        if (c->health <= 0) continue;
        if (bestSlot < 0 || c->player_number < bestSlot) {
            bestSlot = c->player_number;
        }
    }
    spectatorTarget_ = bestSlot;
}

void PlayerView::cycleSpectator(int delta)
{
    if (!entity_group) {
        return;
    }
    auto self = entity_group->getCharacter(playernumber);
    // Cycling is a spectator-only affordance. While we're alive we own
    // the camera and the dpad belongs to character movement.
    if (self && self->health > 0) {
        return;
    }
    // Snapshot the eligible surviving teammates in slot order so a
    // positive `delta` always advances and -1 always retreats, with
    // wraparound at the ends.
    std::vector<int> slots;
    for (const auto& c : entity_group->getCharacters()) {
        if (!c) continue;
        if (c->isVillain()) continue;
        if (c->player_number == playernumber) continue;
        if (c->health <= 0) continue;
        slots.push_back(c->player_number);
    }
    if (slots.empty()) {
        spectatorTarget_ = -1;
        return;
    }
    std::sort(slots.begin(), slots.end());
    int idx = -1;
    for (size_t i = 0; i < slots.size(); ++i) {
        if (slots[i] == spectatorTarget_) {
            idx = static_cast<int>(i);
            break;
        }
    }
    if (idx < 0) {
        // Current target isn't (or is no longer) in the survivor set --
        // snap to the first one in the direction of travel.
        spectatorTarget_ = delta >= 0 ? slots.front() : slots.back();
        return;
    }
    const int n = static_cast<int>(slots.size());
    int next = ((idx + delta) % n + n) % n;
    spectatorTarget_ = slots[next];
}
