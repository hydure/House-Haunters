#include "HouseHaunters.hpp"
#include "engine/Random.hpp"
#include "engine/NetworkManager.hpp"
#include "engine/Env.hpp"
#include "engine/ModConfig.hpp"
#include "engine/Paths.hpp"
#include <string>

////////////////////////
// HouseHaunters.cpp
//
// Game setup: register screens with the engine and pick the starting one.
//
// Next check out the file include/game/screens/GameplayScreen.hpp
///////////////////////

void HouseHauntersGame::init()
{
    config = std::make_shared<Config>();

    // Load player-facing mod data (character stats, sprite paths, lobby
    // screen layout, villain sprite) before any screen is constructed.
    // A missing or malformed mods.xml is non-fatal: ModConfig keeps its
    // baked-in defaults which reproduce the vanilla art and stats.
    ModConfig::instance().loadFromFile(Paths::resource("mods.xml"));

    // Networked multiplayer entry point (item #15). Parses HH_NET; in
    // OFFLINE mode every NetworkManager call below is a no-op and the
    // local couch-co-op path runs unchanged.
    auto& net = NetworkManager::instance();
    net.configureFromEnv();
    if (net.isNetworked()) {
        if (net.connectAndHandshake()) {
            // Every peer must seed its RNG from the host-broadcast value
            // so PlantSeeds-driven streams stay aligned. This replaces
            // the offline "seed from system clock" path.
            PlantSeeds(net.seed());

            // Pre-fill the configuration the gameplay screen normally
            // gets from the character-select flow. Slot i -> player (i+1),
            // characters assigned deterministically by slot id (BRO, SIS,
            // DAD, MOM). A real networked character-select is a separate
            // UX task; see NetworkManager.hpp for the scope limits.
            config->num_players = net.totalPlayers();
            config->player_map.clear();
            config->char_map.clear();
            for (int slot = 0; slot < config->num_players; ++slot) {
                config->player_map[slot]      = slot + 1;
                config->char_map[slot + 1]    =
                    static_cast<Config::CHARACTER>(slot % 4);
            }
        }
        else {
            // Handshake failed -> downgraded to OFFLINE inside NM. Seed
            // normally so the local fallback still plays.
            PlantSeeds(-1);
        }
    }
    else {
        // Seed the shared RNG once at startup. Negative seed means "use the
        // system clock". All randomInt(...) calls (engine/RandomUtil.hpp)
        // route through this stream.
        PlantSeeds(-1);
    }

    this->setName("House Haunters");
    // Window position + dimensions.
    this->setWindowRect(100, 100, config->width, config->height);
    // Hand the engine the target frame rate so it can pace the simulation.
    this->setTargetFps(config->fps);
    // Open in fullscreen if Config says so (set by --fullscreen CLI flag
    // or HH_FULLSCREEN=1). F11 / Alt+Enter toggles at runtime regardless.
    this->setFullscreen(config->fullscreen);

    // Build each screen and hand it the shared config.
    auto screen_gamestory = std::make_unique<GamestoryScreen>();
    screen_gamestory->setConfig(config);
    auto screen_gameplay  = std::make_unique<GameplayScreen>();
    screen_gameplay->setConfig(config);
    auto screen_gametitle = std::make_unique<GametitleScreen>();
    screen_gametitle->setConfig(config);
    auto screen_character = std::make_unique<CharacterScreen>();
    screen_character->setConfig(config);
    auto screen_controls  = std::make_unique<ControlsScreen>();
    screen_controls->setConfig(config);
    auto screen_end       = std::make_unique<EndGameScreen>();
    screen_end->setConfig(config);
    auto screen_host      = std::make_unique<HostScreen>();
    screen_host->setConfig(config);
    auto screen_join      = std::make_unique<JoinScreen>();
    screen_join->setConfig(config);

    // Register each screen by name (unique_ptr must be moved in).
    this->addGameScreen("Story",     std::move(screen_gamestory));
    this->addGameScreen("Title",     std::move(screen_gametitle));
    this->addGameScreen("Character", std::move(screen_character));
    this->addGameScreen("Controls",  std::move(screen_controls));
    this->addGameScreen("GamePlay",  std::move(screen_gameplay));
    this->addGameScreen("GameEnd",   std::move(screen_end));
    this->addGameScreen("Host",      std::move(screen_host));
    this->addGameScreen("Join",      std::move(screen_join));

    // Open on the story screen -- unless we're networked, in which case
    // we skip the lobby flow (story/title/character-select) and jump
    // straight into gameplay with the synchronized player config above.
    // HH_TEST_BYPASS=1 forces the same straight-to-GamePlay path for
    // offline runs (used by the diagnostic screenshot harness).
    const bool testBypass = Env::asBool("HH_TEST_BYPASS");
    if (testBypass && !NetworkManager::instance().isNetworked()) {
        config->num_players = 1;
        config->player_map.clear();
        config->char_map.clear();
        config->player_map[0] = 1;
        config->char_map[1]   = Config::BRO;
        this->changeGameScreen("GamePlay");
    }
    else if (NetworkManager::instance().isNetworked()) {
        this->changeGameScreen("GamePlay");
    }
    else {
        this->changeGameScreen("Story");
    }
}
