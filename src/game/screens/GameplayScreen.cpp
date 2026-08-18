#include "game/screens/GameplayScreen.hpp"
#include "engine/RandomUtil.hpp"
#include "engine/Paths.hpp"
#include "engine/ModConfig.hpp"
#include "engine/ClueReader.hpp"
#include "engine/NetworkManager.hpp"
#include "game/InvestigationJournal.hpp"
#include "game/RunSummary.hpp"
#include "game/SpectatorSupport.hpp"
#include "game/WeaponSystem.hpp"
#include "game/characters/Character.hpp"
#include "game/characters/Villain.hpp"
#include "game/objects/Clue.hpp"
#include <algorithm>

void GameplayScreen::init()
{
    hunt.load(Paths::resource(ModConfig::instance().audio().gameplay_music));

    InvestigationJournal::instance().reset();
    SpectatorSupport::instance().reset();
    clock.restart();
    this->views.clear();
    entity_group = EntityGroup();

    // Pull the latest player count before sizing the room grid.
    num_players = config->num_players;
    // Room density scales (roughly) with the number of players, then is
    // multiplied by a difficulty factor so harder difficulties mean a larger
    // house to search.
    int basePerPlayers = 20;
    switch (num_players) {
        case 1: basePerPlayers = 20;  break;
        case 2: basePerPlayers = 40;  break;
        case 3: basePerPlayers = 60;  break;
        case 4: basePerPlayers = 100; break;
        default: basePerPlayers = 20; break;
    }
    float difficultyMult = 1.0f;
    switch (config->difficulty) {
        case Config::EASY:   difficultyMult = 0.6f; break;
        case Config::NORMAL: difficultyMult = 1.0f; break;
        case Config::HARD:   difficultyMult = 1.5f; break;
    }
    const int roomCount = static_cast<int>(basePerPlayers * difficultyMult);
    group.generateRoomGrid(roomCount > 0 ? roomCount : basePerPlayers);
    RunSummary::instance().begin(config->difficulty, num_players, group.roomCount());

    // Phase-1 timer (time spent searching for clues before the villain
    // spawns) scales INVERSELY with difficulty -- easier games give you
    // longer to find a weapon, harder games rush the ghost arrival.
    // Multipliers chosen to bracket the existing 90s default.
    float phaseTimeMult = 1.0f;
    switch (config->difficulty) {
        case Config::EASY:   phaseTimeMult = 1.33f; break; // ~120s at default
        case Config::NORMAL: phaseTimeMult = 1.0f;  break; //  ~90s
        case Config::HARD:   phaseTimeMult = 0.66f; break; //  ~60s
    }
    // 90.f matches the Config default; reading from config preserves any
    // future override (e.g. a tuned starting value loaded from disk).
    const float basePhaseTime = config->time_Per_Phase > 0.f
        ? config->time_Per_Phase
        : 90.f;
    phaseTime = basePhaseTime * phaseTimeMult;

    this->createClues();
    // PlayerView mustn't set its own viewport, or createViews would have to
    // re-run that bookkeeping on every player.
    this->createViews(num_players);
    entity_group.init();
    // Health snapshots must be applied AFTER entity_group.init() --
    // Character::init() blanks health to the per-class default, so any
    // earlier overwrite gets stomped.
    applyHealthSnapshots();

    // RAII subscription: released when the engine leaves this screen.
    this->subscribe("player_died", [this](base_event_type /*e*/) {
        RunSummary::instance().recordDeath();
        if (--num_players == 0) {
            RunSummary::instance().finish(
                false,
                static_cast<int>(InvestigationJournal::instance().entries().size()));
            auto event = std::make_shared< Event<std::string> >("GameEnd");
            Events::queueEvent("change_screen", event);
        }
    });

    // In-game pause menu. Created here so it picks up the current engine
    // pointer (for fullscreen toggling + clean shutdown via exit()) and
    // so its SFML resources are allocated against the active context.
    pauseMenu.init(this->engine);

    // Watch for the MENU button (Esc on keyboard, Start on a gamepad)
    // to toggle the overlay. Also forward all gamepad events to the
    // menu while it's open so it can drive its own navigation. The menu
    // sets PauseMenu::inputBlocked() while open; PlayerView checks that
    // flag and stops forwarding events to the player Character, so the
    // game freezes cleanly behind the overlay.
    this->subscribe("gamepad_event", [this](base_event_type e) {
        auto gpe = dynamic_cast< GamepadEvent& >(*e);
        if (gpe.type == GamepadEvent::RELEASED && gpe.button == "MENU") {
            if (pauseMenu.isOpen()) {
                pauseMenu.close();
            }
            else {
                // Zero out player movement state before suspending input
                // so a key held when the player opened the menu doesn't
                // leave the character walking when they resume. Speed
                // boosts (BRO sprint) self-correct on the next press of
                // X; not worth chasing in this first iteration.
                bool broActive = false;
                for (const auto& c : entity_group.getCharacters()) {
                    if (c) {
                        c->direction = sf::Vector2f(0.f, 0.f);
                        if (c->character == Config::CHARACTER::BRO) {
                            broActive = true;
                        }
                    }
                }
                // Hide the BRO sprint hint on the CONTROLS page when
                // no party member can actually use it -- avoids a
                // misleading "press X to sprint" line for MOM/DAD/SIS.
                pauseMenu.setBroActive(broActive);
                pauseMenu.open();
            }
            return;
        }
        if (pauseMenu.isOpen()) {
            pauseMenu.handleEvent(gpe);
        }
    });

    // Build the always-visible "Press ESC for menu" hint once. Drawn in
    // the bottom-right corner of the logical 720x480 canvas. Allocating
    // the font here keeps onDraw allocation-free.
    menuHint.setFont(*ResourceManager::getFont(
        Paths::resource("fonts/Underdog-Regular.ttf")));
    menuHint.setString("Press ESC for menu");
    menuHint.setCharacterSize(13);
    menuHint.setFillColor(sf::Color(230, 230, 230, 200));
    menuHint.setStyle(sf::Text::Italic);
    {
        sf::FloatRect b = menuHint.getLocalBounds();
        // Bottom-right anchored, with a small inset so the text doesn't
        // touch the window border.
        menuHint.setOrigin(b.left + b.width, b.top + b.height);
        menuHint.setPosition(720.f - 8.f, 480.f - 6.f);
    }
    menuHintReady = true;
}

bool GameplayScreen::onExit()
{
    // Release per-player views (and their RAII gamepad subscriptions) before
    // the engine switches away. Keeps stale views from receiving events.
    this->views.clear();
    // Force-close the menu so its static input-block flag doesn't leak
    // into the next screen (e.g. nothing on the title screen would
    // respond if we left it set).
    pauseMenu.close();
    return true;
}

void GameplayScreen::createClues()
{
    // RNG already seeded in HouseHauntersGame::init via PlantSeeds.
    reader.readFile(Paths::resource("items.xml"));
    reader.selectItems();
    WeaponSystem::instance().configure(
        reader.getItemHigh().type, reader.getItemLow().type);

    for (const auto& r : group.rooms) {
        const sf::Vector2f roomPos = r->rect.getPosition();

        for (const sf::IntRect& tile : r->clueCoordinates) {
            clue = std::make_unique<Clue>();
            clue->setRoomGroup(&group);
            clue->setEntities(&entity_group);

            hiLow = randomInt(2);
            clue->clueJackpot   = reader.getCluesJackpot()[hiLow];
            clue->clueSpec      = reader.getCluesSpec()[hiLow];
            clue->clueVague     = reader.getCluesVague()[hiLow];
            clue->clueWorthless = reader.getCluesWorthless()[hiLow];
            clue->highLow       = hiLow;

            const int randint = randomInt(100);
            if (randint <= 50) {
                clue->setClue = clue->clueWorthless;
            } else if (randint <= 80) {
                clue->setClue = clue->clueVague;
            } else if (randint <= 95) {
                clue->setClue = clue->clueSpec;
            } else {
                clue->setClue = clue->clueJackpot;
                clue->activatedItem = false;
            }

            // tile.{left,top,width,height} are in tile-grid units (32 px / tile).
            const int x      = static_cast<int>(roomPos.x) + 32 * tile.left;
            const int y      = static_cast<int>(roomPos.y) + 32 * tile.top;
            const int width  = 32 * tile.width;
            const int height = 32 * tile.height;

            clue->setCoordinates(x, y, width, height);
            clue->init();
            // Cache the non-owning observer pointer in the room before
            // transferring ownership -- Character::checkClues walks this
            // per-room list instead of every clue in the house.
            r->cluesInRoom.push_back(clue.get());
            entity_group.addClue(std::move(clue));
        }
    }
}

void GameplayScreen::createViews(int numPlayers)
{
    for (int i = 0; i < numPlayers; i++) {
        // initInPlace=false: the entity_group.init() call below in
        // init() will fan out and call Character::init() in one pass.
        spawnPlayerForSlot(i + 1, numPlayers, /*initInPlace=*/false);
    }
}

void GameplayScreen::spawnPlayerForSlot(int playernum, int numPlayers, bool initInPlace)
{
    double ratio_w = 1.0;
    double ratio_h = 1.0;
    const double gutter  = 5.0; // pixels between player views
    const double gutterx = gutter / 720.0 / 2;
    const double guttery = gutter / 480.0 / 2;

    if (numPlayers >= 3) {
        ratio_w /= 2;
        ratio_h /= 2;
    }
    else if (numPlayers == 2) {
        ratio_w /= 2;
    }

    const int idx = playernum - 1;
    const int x = idx % 2;
    const int y = idx / 2;

    // Find this player's gamepad index (-1 if not yet bound).
    int gamepad_index = -1;
    auto found = std::find_if(
        config->player_map.begin(), config->player_map.end(),
        [=](const std::pair<int, int>& kv) { return kv.second == playernum; });
    if (found != config->player_map.end()) {
        gamepad_index = found->first;
    }

    auto view = std::make_unique<PlayerView>();
    view->setRoomGroup(&group);
    view->numPlayers = numPlayers;
    view->setView(
        sf::FloatRect(0.f, 0.f,
                      static_cast<float>(720 * ratio_w),
                      static_cast<float>(480 * ratio_h)),
        sf::FloatRect(
            static_cast<float>((ratio_w + gutterx) * x),
            static_cast<float>((ratio_h + guttery) * y),
            static_cast<float>(ratio_w - gutterx * (1.0 - x)),
            static_cast<float>(ratio_h - guttery * (1.0 - y))));

    auto character = std::make_shared<Character>();
    character->setRoomGroup(&group);
    character->setPlayerNumber(playernum);
    character->setEntities(&entity_group);
    // Assumes config->char_map has an entry for every player.
    character->setCharacter(config->char_map[playernum]);
    character->setGamepadIndex(gamepad_index);
    Character* charPtr = character.get();
    entity_group.addCharacter(std::move(character));

    if (initInPlace) {
        charPtr->init();
        // Restore HP for a mid-game rejoin -- inline because there's no
        // central applyHealthSnapshots() pass on this path.
        const int snapHp = NetworkManager::instance().slotHealthSnapshot(playernum);
        if (shouldApplyHealthSnapshot(snapHp)) {
            charPtr->health = snapHp;
            if (charPtr->health > charPtr->maxHealth) {
                charPtr->health = charPtr->maxHealth;
            }
        }
    }

    view->setEntities(&entity_group);
    view->setEntityNumber(playernum);
    view->init();
    this->views.push_back(std::move(view));
}

void GameplayScreen::applyHealthSnapshots()
{
    auto& net = NetworkManager::instance();
    for (const auto& c : entity_group.getCharacters()) {
        if (!c) continue;
        if (c->isVillain()) continue;
        if (c->player_number <= 0) continue;
        const int snap = net.slotHealthSnapshot(c->player_number);
        if (!shouldApplyHealthSnapshot(snap)) continue;
        c->health = snap;
        if (c->health > c->maxHealth) {
            c->health = c->maxHealth;
        }
    }
}

void GameplayScreen::acceptLateJoiners()
{
    auto& net = NetworkManager::instance();
    if (!net.isNetworked() || net.mode() != NetworkManager::Mode::HOST) {
        return;
    }
    // Keep accepting any peer that's waiting on the listener -- the host
    // started the game without waiting for the lobby to fill, so this is
    // the only place a mid-game joiner can finish their handshake.
    bool err = false;
    net.pollAccept(&err);

    while (true) {
        const int slot = net.newlyJoinedSlot();
        if (slot <= 0) break;
        // Round out config so the new character can be created. If the
        // character-pick screen already populated char_map with a value
        // for this slot we keep it; otherwise fall back to the slot id
        // mod 4 (BRO,SIS,DAD,MOM cycling).
        if (config->char_map.count(slot) == 0) {
            config->char_map[slot] = static_cast<Config::CHARACTER>(slot % 4);
        }
        if (config->player_map.count(slot - 1) == 0) {
            config->player_map[slot - 1] = slot;
        }
        // num_players in this screen tracks live-and-playing slots; bump
        // it before spawning so the new PlayerView sizes its viewport
        // against the post-join count.
        ++num_players;
        config->num_players = num_players;
        spawnPlayerForSlot(slot, num_players, /*initInPlace=*/true);
    }
}

void GameplayScreen::recordHealthSnapshots()
{
    auto& net = NetworkManager::instance();
    for (const auto& c : entity_group.getCharacters()) {
        if (!c) continue;
        if (c->isVillain()) continue;
        if (c->player_number <= 0) continue;
        net.recordSlotHealth(c->player_number, c->health);
    }
}

void GameplayScreen::onUpdate(float dt)
{
    // Freeze the world while the pause menu is up: skip per-view, room,
    // and entity updates plus the phase timer. The menu itself reacts
    // to gamepad events directly (queued through the event bus) so it
    // doesn't need a per-frame tick.
    if (pauseMenu.isOpen()) {
        return;
    }

    RunSummary::instance().update(dt);

    // Service the listener / spawn any new player BEFORE per-view updates
    // so a brand-new view sees this tick instead of waiting a frame.
    acceptLateJoiners();

    for (const auto& v : views) {
        v->update(dt);
    }
    group.update(dt);
    entity_group.update(dt);
    SpectatorSupport::instance().update(dt);

    // Keep the per-slot HP snapshot fresh so a player who disconnects
    // right now and reconnects later spawns back with the exact HP they
    // had this frame.
    recordHealthSnapshots();

    if (clock.getElapsedTime().asSeconds() >= phaseTime) {
        if (phase == 1) {
            hunt.play();
            ghost = std::make_shared<Villain>();
            ghost->setPlayerNumber(-1);
            ghost->setRoomGroup(&group);
            ghost->setEntities(&entity_group);
            // Wire the ghost's Director-ping cadence to the configured
            // difficulty: EASY = rare pings (stale info, easy to kite),
            // HARD = frequent pings (the ghost re-targets every couple
            // of seconds).
            ghost->setDifficulty(config->difficulty);
            ghost->init();
            entity_group.addCharacter(std::move(ghost));
            phase++;
        }
        clock.restart();
    }
}

void GameplayScreen::onDraw(sf::RenderTarget& ctx, sf::RenderStates /*states*/) const
{
    for (const auto& v : views) {
        ctx.draw(*v);
    }

    // Bottom-right hint -- always drawn during gameplay so a player can
    // see how to bring up the menu without consulting a manual. Hidden
    // while the menu itself is up to avoid visual noise behind the
    // overlay.
    if (menuHintReady && !pauseMenu.isOpen()) {
        const sf::View saved = ctx.getView();
        ctx.setView(ctx.getDefaultView());
        ctx.draw(menuHint);
        ctx.setView(saved);
    }

    // Pause overlay sits on top of everything else (including the hint).
    pauseMenu.draw(ctx);
}
