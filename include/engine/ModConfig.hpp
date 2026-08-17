#ifndef MOD_CONFIG_HPP
#define MOD_CONFIG_HPP

#include <array>
#include <string>
#include <vector>
#include <SFML/Graphics/Color.hpp>

////////////////////////////////
// ModConfig.hpp
//
// Data-driven overrides for the player-facing presentation layer:
//
//   * The character-selection lobby (title text, portrait atlas + slicing,
//     screen layout, background color)
//   * Each playable character's stats and sprite sheet (BRO, SIS, DAD, MOM)
//   * The villain's sprite sheet, frame size, walk-animation indices, and
//     starting health
//
// Data lives in resources/mods.xml. If the file is missing, unreadable, or
// malformed the loader silently falls back to defaults that exactly match
// the values shipped with the vanilla game, so an unmodded launch always
// just works.
//
// The class is a singleton because the consumers (Character::init,
// Villain::init, CharacterScreen::init, CharacterSelection::setPortrait)
// are all constructed deep inside the engine's screen/object graph and
// would otherwise need a plumbing-only reference passed through several
// layers. Tests get isolation via reset() + loadFromString().
////////////////////////////////

class ModConfig {
public:
    struct CharacterMod {
        // Lobby display: which 2*N slot of the portrait atlas to show.
        int portrait_index = 0;
        // Combat / movement stats.
        int health = 3;
        int max_health = 3;
        // Multiplier applied to Character::speed (defaults to 120 in
        // Character.hpp). 1.0 = unchanged.
        double speed_multiplier = 1.0;
        // Which 2x2 slot of the character spritesheet this character lives
        // in. The Character::init code converts it to a frame offset:
        //   x = slot % 2;  y = slot / 2;  mod = 3*x + 24*y;
        int sprite_location = 0;
        // Texture this character's walk frames are sampled from. Path is
        // relative to resources/ (Paths::resource() is applied at use).
        std::string sprite_sheet_path = "sprites/character_sheet.png";
    };

    struct VillainMod {
        std::string sprite_sheet_path = "sprites/ghost.png";
        int frame_width = 32;
        int frame_height = 48;
        int health = 10;
        // Per-direction walk frame indices into the spritesheet. The
        // vanilla ghost sheet has 12 frames laid out in 4 rows of 3 frames
        // each (down/left/right/up); each direction loops {a,b,a,c} for a
        // classic ping-pong walk cycle.
        std::vector<int> walk_down  = {1, 2, 1, 0};
        std::vector<int> walk_left  = {4, 5, 4, 3};
        std::vector<int> walk_right = {7, 8, 7, 6};
        std::vector<int> walk_up    = {10, 11, 10, 9};
    };

    struct ScreenMod {
        // Title line above the portrait row.
        std::string title_text = "MAKE YOUR TEAM";
        std::string title_font = "fonts/Underdog-Regular.ttf";
        int  title_size = 24;
        int  title_x = 250;
        int  title_y = 50;
        // Background fill for the whole screen.
        sf::Color background_color{30, 30, 30};
        // Portrait atlas configuration.
        std::string portrait_atlas = "HH_Portraits.png";
        int   portrait_tile_w   = 256;
        int   portrait_tile_h   = 384;
        int   portrait_atlas_cols = 4;
        float portrait_scale = 0.586f;
        // Where the leftmost portrait sits and how much horizontal room
        // each one gets. Spacing replicates the historic
        //   (720 - 4*15) / 4 ~= 165
        // gap so an unmodded lobby renders pixel-for-pixel as before.
        float layout_start_x = 40.0f;
        float layout_start_y = 120.0f;
        float layout_x_spacing = 165.0f;
    };

    // Player-replaceable audio cues. Every field is a path relative to
    // resources/ (Paths::resource() is applied at use), matching the
    // sprite-path convention used by CharacterMod / VillainMod. SFML
    // loads .ogg / .flac / .wav out of the box; .mp3 is NOT supported
    // (libmpg123 is not bundled with SFML 2.x for licensing reasons).
    //
    // Defaults match the historic hardcoded paths so an unmodded launch
    // sounds exactly like the vanilla game. A missing/unreadable file at
    // runtime is non-fatal (the consumer ignores the load failure and
    // simply plays nothing for that cue) so a typo in mods.xml can't
    // silence the entire game.
    struct AudioMod {
        // Long-form looping music streamed off disk (sf::Music).
        // `title_music` ships empty -- the title screen is silent by
        // default. Mods can opt in to a title loop by setting
        // <track id="title_music" path="music/your_loop.ogg"/> in
        // mods.xml. (The vanilla loop used to be a 16 MB FLAC; it was
        // dropped to keep the standalone HH.exe at a reasonable size.)
        std::string title_music    = "";                   // GametitleScreen loop (off by default).
        std::string gameplay_music = "music/start.ogg";    // GameplayScreen "hunt" cue.
        std::string endgame_music  = "music/gameover.flac";// EndGameScreen sting.

        // Short one-shot sound effects loaded into memory (sf::Sound).
        std::string lobby_select_sfx = "music/thunder.flac"; // Character selected.
        std::string player_hurt_sfx  = "music/hurt.wav";     // Player takes a hit.
        std::string player_death_sfx = "music/dead.wav";     // Player dies.
        std::string ghost_chase_sfx  = "music/chase.wav";    // Ghost lunge cue.
        std::string clue_found_sfx   = "music/curse.wav";    // New case-file evidence.
        std::string weapon_select_sfx = "music/loud.wav";    // Weapon category changed.
        std::string haunt_rise_sfx   = "music/near.flac";    // Haunt pressure increased.
    };

    // Singleton accessor. Constructed once with all-default values.
    static ModConfig& instance();

    // Parse from disk / memory. Both return true on a clean parse. A
    // missing file or syntactically bad XML returns false; in either case
    // the in-memory mod data is left at its defaults so the game keeps
    // working.
    bool loadFromFile(const std::string& path);
    bool loadFromString(const std::string& xml);

    // Drop any loaded overrides and revert to vanilla defaults. Tests
    // call this between cases to keep the singleton from leaking state.
    void reset();

    bool isLoaded() const { return loaded_; }
    const std::string& lastError() const { return lastError_; }

    // Character index matches the Config::CHARACTER enum (0=BRO, 1=SIS,
    // 2=DAD, 3=MOM). Out-of-range indices clamp to 0 rather than throw so
    // a malformed mods.xml can't crash the game on the first frame.
    const CharacterMod& character(int index) const;

    const VillainMod& villain() const { return villain_; }
    const ScreenMod&  screen()  const { return screen_;  }
    const AudioMod&   audio()   const { return audio_;   }

private:
    ModConfig();
    void applyDefaults();
    bool parseBuffer(char* mutableBuffer);

    std::array<CharacterMod, 4> characters_;
    VillainMod villain_;
    ScreenMod screen_;
    AudioMod  audio_;
    bool loaded_ = false;
    std::string lastError_;
};

#endif
