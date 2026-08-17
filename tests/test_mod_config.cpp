// ModConfig regression tests.
//
// Exercises the player-facing modding surface:
//   * Defaults reproduce the vanilla character / villain / lobby values
//     so an unmodded game is unchanged.
//   * resources/mods.xml is a real file that parses cleanly and produces
//     those same defaults (the shipped mod file is itself documentation).
//   * Custom XML overrides override individual fields and leave the rest
//     at defaults (partial mods are first-class).
//   * Malformed XML and missing files are non-fatal -- the loader returns
//     false but leaves the in-memory config at sane defaults.
//   * Villain walk-frame CSV parsing (whitespace, malformed items).
//   * reset() clears state between tests so the singleton doesn't leak
//     across cases.

#include "test_harness.hpp"
#include "engine/ModConfig.hpp"
#include "engine/ResourceFS.hpp"

#include <string>

namespace {

// Logical resource key, identical to what HouseHaunters.cpp passes via
// Paths::resource("mods.xml"). ModConfig::loadFromFile routes through
// ResourceFS internally so the same string works in disk + embedded
// builds.
const char* kVanillaModPath = "mods.xml";

// Helper: pristine state for every test. Many cases load custom XML and
// we don't want side-effects bleeding to the next case.
void freshConfig()
{
    ModConfig::instance().reset();
}

} // namespace

TEST_CASE("ModConfig: defaults match the historic Character::init switch table")
{
    freshConfig();
    const auto& mc = ModConfig::instance();
    CHECK(!mc.isLoaded());

    // BRO: sprite slot 2, hp 3, speed_mul 1.0, portrait 0
    CHECK_EQ(mc.character(0).sprite_location,   2);
    CHECK_EQ(mc.character(0).health,            3);
    CHECK_EQ(mc.character(0).max_health,        3);
    CHECK_EQ(mc.character(0).portrait_index,    0);

    // SIS: sprite slot 1, hp 3, portrait 2
    CHECK_EQ(mc.character(1).sprite_location,   1);
    CHECK_EQ(mc.character(1).health,            3);
    CHECK_EQ(mc.character(1).portrait_index,    2);

    // DAD: sprite slot 3, hp 5 (the only character with extra HP), portrait 4
    CHECK_EQ(mc.character(2).sprite_location,   3);
    CHECK_EQ(mc.character(2).health,            5);
    CHECK_EQ(mc.character(2).max_health,        5);
    CHECK_EQ(mc.character(2).portrait_index,    4);

    // MOM: sprite slot 0, hp 3, speed_mul 0.85 (only character that
    // changes Character::speed), portrait 6
    CHECK_EQ(mc.character(3).sprite_location,   0);
    CHECK_EQ(mc.character(3).health,            3);
    CHECK_EQ(mc.character(3).portrait_index,    6);
    CHECK(mc.character(3).speed_multiplier < 0.86);
    CHECK(mc.character(3).speed_multiplier > 0.84);
}

TEST_CASE("ModConfig: villain defaults reproduce the original Villain::init")
{
    freshConfig();
    const auto& v = ModConfig::instance().villain();
    CHECK_EQ(v.sprite_sheet_path, std::string("sprites/ghost.png"));
    CHECK_EQ(v.frame_width,  32);
    CHECK_EQ(v.frame_height, 48);
    CHECK_EQ(v.health, 10);
    CHECK_EQ(v.walk_down.size(),  static_cast<size_t>(4));
    CHECK_EQ(v.walk_down[0], 1);
    CHECK_EQ(v.walk_down[1], 2);
    CHECK_EQ(v.walk_down[2], 1);
    CHECK_EQ(v.walk_down[3], 0);
    CHECK_EQ(v.walk_left[0], 4);
    CHECK_EQ(v.walk_right[0], 7);
    CHECK_EQ(v.walk_up[0], 10);
}

TEST_CASE("ModConfig: screen defaults reproduce the original lobby layout")
{
    freshConfig();
    const auto& s = ModConfig::instance().screen();
    CHECK_EQ(s.title_text, std::string("MAKE YOUR TEAM"));
    CHECK_EQ(s.title_font, std::string("fonts/Underdog-Regular.ttf"));
    CHECK_EQ(s.title_size, 24);
    CHECK_EQ(s.title_x, 250);
    CHECK_EQ(s.title_y, 50);
    CHECK_EQ(static_cast<int>(s.background_color.r), 30);
    CHECK_EQ(static_cast<int>(s.background_color.g), 30);
    CHECK_EQ(static_cast<int>(s.background_color.b), 30);
    CHECK_EQ(s.portrait_atlas, std::string("HH_Portraits.png"));
    CHECK_EQ(s.portrait_tile_w, 256);
    CHECK_EQ(s.portrait_tile_h, 384);
    CHECK_EQ(s.portrait_atlas_cols, 4);
    // Float compare with tolerance to avoid the usual literal-vs-stored
    // representation drift.
    CHECK(s.portrait_scale > 0.585f && s.portrait_scale < 0.587f);
}

TEST_CASE("ModConfig: out-of-range character index falls back to BRO")
{
    freshConfig();
    const auto& low  = ModConfig::instance().character(-1);
    const auto& high = ModConfig::instance().character(99);
    CHECK_EQ(low.sprite_location,  2);
    CHECK_EQ(high.sprite_location, 2);
}

TEST_CASE("ModConfig: shipped resources/mods.xml parses cleanly and reproduces vanilla")
{
    freshConfig();
    REQUIRE(hh::ResourceFS::exists(kVanillaModPath));
    const bool ok = ModConfig::instance().loadFromFile(kVanillaModPath);
    if (!ok) {
        // Surface why so a future mods.xml typo doesn't fail silently in CI.
        std::cerr << "    parse error: "
                  << ModConfig::instance().lastError() << std::endl;
    }
    CHECK(ok);
    CHECK(ModConfig::instance().isLoaded());

    // After loading the shipped file, every value must still equal the
    // baked-in defaults (mods.xml IS the documentation; if it ever drifts
    // from defaults the player would get surprising changes).
    const auto& mc = ModConfig::instance();
    CHECK_EQ(mc.character(0).sprite_location, 2); // BRO
    CHECK_EQ(mc.character(2).health,          5); // DAD
    CHECK(mc.character(3).speed_multiplier < 0.86 &&
          mc.character(3).speed_multiplier > 0.84); // MOM
    CHECK_EQ(mc.villain().health, 10);
    CHECK_EQ(mc.screen().title_text, std::string("MAKE YOUR TEAM"));
    freshConfig();
}

TEST_CASE("ModConfig: missing file returns false and leaves defaults intact")
{
    freshConfig();
    const bool ok = ModConfig::instance().loadFromFile(
        "definitely/does/not/exist/mods.xml");
    CHECK(!ok);
    CHECK(!ModConfig::instance().isLoaded());
    CHECK(!ModConfig::instance().lastError().empty());
    // Defaults still usable.
    CHECK_EQ(ModConfig::instance().character(2).health, 5);
}

TEST_CASE("ModConfig: malformed XML returns false and leaves defaults intact")
{
    freshConfig();
    const bool ok = ModConfig::instance().loadFromString(
        "<mods><unclosed>");
    CHECK(!ok);
    // Defaults still pristine even though we tried to parse garbage.
    CHECK_EQ(ModConfig::instance().villain().health, 10);
    CHECK_EQ(ModConfig::instance().screen().title_text,
             std::string("MAKE YOUR TEAM"));
}

TEST_CASE("ModConfig: empty input returns false")
{
    freshConfig();
    CHECK(!ModConfig::instance().loadFromString(""));
}

TEST_CASE("ModConfig: missing <mods> root returns false")
{
    freshConfig();
    const bool ok = ModConfig::instance().loadFromString(
        "<?xml version=\"1.0\"?><other/>");
    CHECK(!ok);
    CHECK(!ModConfig::instance().lastError().empty());
}

TEST_CASE("ModConfig: custom title text overrides the default")
{
    freshConfig();
    const std::string xml =
        "<mods><screen>"
        "<title text=\"CHOOSE YOUR HORROR\" font=\"fonts/myfont.ttf\" "
        "size=\"40\" x=\"100\" y=\"30\"/>"
        "</screen></mods>";
    REQUIRE(ModConfig::instance().loadFromString(xml));
    const auto& s = ModConfig::instance().screen();
    CHECK_EQ(s.title_text, std::string("CHOOSE YOUR HORROR"));
    CHECK_EQ(s.title_font, std::string("fonts/myfont.ttf"));
    CHECK_EQ(s.title_size, 40);
    CHECK_EQ(s.title_x, 100);
    CHECK_EQ(s.title_y, 30);
    // Unspecified fields stay at defaults.
    CHECK_EQ(s.portrait_atlas, std::string("HH_Portraits.png"));
    freshConfig();
}

TEST_CASE("ModConfig: custom background color clamps out-of-range channels")
{
    freshConfig();
    const std::string xml =
        "<mods><screen>"
        "<background r=\"-50\" g=\"500\" b=\"128\"/>"
        "</screen></mods>";
    REQUIRE(ModConfig::instance().loadFromString(xml));
    const auto& bg = ModConfig::instance().screen().background_color;
    CHECK_EQ(static_cast<int>(bg.r),   0);
    CHECK_EQ(static_cast<int>(bg.g), 255);
    CHECK_EQ(static_cast<int>(bg.b), 128);
    freshConfig();
}

TEST_CASE("ModConfig: per-character override hits only that character")
{
    freshConfig();
    // Boost DAD's HP and give him a custom sheet. Everyone else should
    // stay at their vanilla numbers.
    const std::string xml =
        "<mods><characters>"
        "<character id=\"DAD\" hp=\"99\" sprite_sheet=\"sprites/dad_mod.png\""
        "           sprite_location=\"1\" portrait_index=\"3\"/>"
        "</characters></mods>";
    REQUIRE(ModConfig::instance().loadFromString(xml));
    const auto& mc = ModConfig::instance();

    CHECK_EQ(mc.character(2).health,           99);
    CHECK_EQ(mc.character(2).max_health,       99); // defaults to hp
    CHECK_EQ(mc.character(2).sprite_location,   1);
    CHECK_EQ(mc.character(2).portrait_index,    3);
    CHECK_EQ(mc.character(2).sprite_sheet_path,
             std::string("sprites/dad_mod.png"));

    // BRO untouched.
    CHECK_EQ(mc.character(0).health,            3);
    CHECK_EQ(mc.character(0).sprite_location,   2);
    CHECK_EQ(mc.character(0).sprite_sheet_path,
             std::string("sprites/character_sheet.png"));

    // MOM untouched.
    CHECK_EQ(mc.character(3).portrait_index,    6);
    freshConfig();
}

TEST_CASE("ModConfig: unknown character id is skipped without affecting known ones")
{
    freshConfig();
    const std::string xml =
        "<mods><characters>"
        "<character id=\"UNCLE\" hp=\"42\"/>"
        "<character id=\"SIS\"   hp=\"7\"/>"
        "</characters></mods>";
    REQUIRE(ModConfig::instance().loadFromString(xml));
    // UNCLE was silently skipped.
    CHECK_EQ(ModConfig::instance().character(1).health, 7); // SIS updated
    CHECK_EQ(ModConfig::instance().character(0).health, 3); // BRO unchanged
    freshConfig();
}

TEST_CASE("ModConfig: villain sprite + walk frame override parses correctly")
{
    freshConfig();
    const std::string xml =
        "<mods><villain sprite_sheet=\"sprites/zombie.png\" "
        "      frame_width=\"48\" frame_height=\"64\" health=\"25\">"
        "  <walk direction=\"down\"  frames=\"0,1,2,3\"/>"
        "  <walk direction=\"left\"  frames=\" 10 , 11 , 12 , 13 \"/>"
        "  <walk direction=\"right\" frames=\"20,21,22,23,24\"/>"
        "  <walk direction=\"up\"    frames=\"30,31,30,32\"/>"
        "</villain></mods>";
    REQUIRE(ModConfig::instance().loadFromString(xml));
    const auto& v = ModConfig::instance().villain();
    CHECK_EQ(v.sprite_sheet_path, std::string("sprites/zombie.png"));
    CHECK_EQ(v.frame_width,  48);
    CHECK_EQ(v.frame_height, 64);
    CHECK_EQ(v.health, 25);

    CHECK_EQ(v.walk_down.size(),  static_cast<size_t>(4));
    CHECK_EQ(v.walk_down[0], 0);
    CHECK_EQ(v.walk_down[3], 3);

    // Whitespace tolerated.
    CHECK_EQ(v.walk_left.size(),  static_cast<size_t>(4));
    CHECK_EQ(v.walk_left[1], 11);

    // Lists of any length accepted (a 5-frame cycle, for instance).
    CHECK_EQ(v.walk_right.size(), static_cast<size_t>(5));
    CHECK_EQ(v.walk_right[4], 24);

    CHECK_EQ(v.walk_up[2], 30);
    freshConfig();
}

TEST_CASE("ModConfig: villain walk override falls back when CSV is empty")
{
    freshConfig();
    // Empty frames="" should leave the default {1,2,1,0} in place rather
    // than clear the animation (a 0-frame anim would crash SpriteAnimation).
    const std::string xml =
        "<mods><villain>"
        "  <walk direction=\"down\" frames=\"\"/>"
        "</villain></mods>";
    REQUIRE(ModConfig::instance().loadFromString(xml));
    const auto& v = ModConfig::instance().villain();
    CHECK_EQ(v.walk_down.size(), static_cast<size_t>(4));
    CHECK_EQ(v.walk_down[0], 1);
    freshConfig();
}

TEST_CASE("ModConfig: reset() restores defaults after a load")
{
    freshConfig();
    const std::string xml =
        "<mods><villain health=\"77\"/></mods>";
    REQUIRE(ModConfig::instance().loadFromString(xml));
    CHECK_EQ(ModConfig::instance().villain().health, 77);

    ModConfig::instance().reset();
    CHECK_EQ(ModConfig::instance().villain().health, 10);
    CHECK(!ModConfig::instance().isLoaded());
}

TEST_CASE("ModConfig: layout override is picked up by the lobby code path")
{
    freshConfig();
    const std::string xml =
        "<mods><screen>"
        "<layout start_x=\"10.5\" start_y=\"200\" x_spacing=\"100\"/>"
        "</screen></mods>";
    REQUIRE(ModConfig::instance().loadFromString(xml));
    const auto& s = ModConfig::instance().screen();
    CHECK(s.layout_start_x > 10.4f && s.layout_start_x < 10.6f);
    CHECK(s.layout_start_y > 199.5f && s.layout_start_y < 200.5f);
    CHECK(s.layout_x_spacing > 99.5f && s.layout_x_spacing < 100.5f);
    freshConfig();
}

TEST_CASE("ModConfig: lowercase / numeric character ids resolve to the right slot")
{
    freshConfig();
    const std::string xml =
        "<mods><characters>"
        "<character id=\"bro\" hp=\"11\"/>"
        "<character id=\"3\"   hp=\"22\"/>"
        "</characters></mods>";
    REQUIRE(ModConfig::instance().loadFromString(xml));
    CHECK_EQ(ModConfig::instance().character(0).health, 11); // bro -> BRO
    CHECK_EQ(ModConfig::instance().character(3).health, 22); // "3" -> MOM
    freshConfig();
}

// =============================================================
// Audio modding -- the <audio><track id=... path=.../></audio>
// block lets players rebind any of the seven in-game cues to a
// different file under resources/. These tests pin both the
// vanilla-default paths and the partial-override semantics so a
// future refactor can't silently move a cue or break a rebind.
// =============================================================

TEST_CASE("ModConfig: audio defaults reproduce the historic hardcoded paths")
{
    freshConfig();
    const auto& a = ModConfig::instance().audio();
    // Streaming sf::Music tracks. title_music ships empty (silent) --
    // the 16 MB vanilla loop was dropped from the bundle; modders can
    // restore one via mods.xml. The other two defaults are unchanged.
    CHECK_EQ(a.title_music,    std::string(""));
    CHECK_EQ(a.gameplay_music, std::string("music/start.ogg"));
    CHECK_EQ(a.endgame_music,  std::string("music/gameover.flac"));
    // One-shot sf::Sound effects.
    CHECK_EQ(a.lobby_select_sfx, std::string("music/thunder.flac"));
    CHECK_EQ(a.player_hurt_sfx,  std::string("music/hurt.wav"));
    CHECK_EQ(a.player_death_sfx, std::string("music/dead.wav"));
    CHECK_EQ(a.ghost_chase_sfx,  std::string("music/chase.wav"));
    CHECK_EQ(a.clue_found_sfx,   std::string("music/curse.wav"));
    CHECK_EQ(a.weapon_select_sfx, std::string("music/loud.wav"));
    CHECK_EQ(a.haunt_rise_sfx,   std::string("music/near.flac"));
}

TEST_CASE("ModConfig: full audio override rebinds every cue")
{
    freshConfig();
    const std::string xml =
        "<mods><audio>"
        "<track id=\"title_music\"    path=\"music/custom_title.ogg\"/>"
        "<track id=\"gameplay_music\" path=\"music/custom_hunt.ogg\"/>"
        "<track id=\"endgame_music\"  path=\"music/custom_end.flac\"/>"
        "<track id=\"lobby_select\"   path=\"music/custom_pick.wav\"/>"
        "<track id=\"player_hurt\"    path=\"music/custom_ouch.wav\"/>"
        "<track id=\"player_death\"   path=\"music/custom_rip.wav\"/>"
        "<track id=\"ghost_chase\"    path=\"music/custom_boo.wav\"/>"
        "<track id=\"clue_found\"     path=\"music/custom_clue.wav\"/>"
        "<track id=\"weapon_select\"  path=\"music/custom_weapon.wav\"/>"
        "<track id=\"haunt_rise\"     path=\"music/custom_haunt.wav\"/>"
        "</audio></mods>";
    REQUIRE(ModConfig::instance().loadFromString(xml));
    const auto& a = ModConfig::instance().audio();
    CHECK_EQ(a.title_music,      std::string("music/custom_title.ogg"));
    CHECK_EQ(a.gameplay_music,   std::string("music/custom_hunt.ogg"));
    CHECK_EQ(a.endgame_music,    std::string("music/custom_end.flac"));
    CHECK_EQ(a.lobby_select_sfx, std::string("music/custom_pick.wav"));
    CHECK_EQ(a.player_hurt_sfx,  std::string("music/custom_ouch.wav"));
    CHECK_EQ(a.player_death_sfx, std::string("music/custom_rip.wav"));
    CHECK_EQ(a.ghost_chase_sfx,  std::string("music/custom_boo.wav"));
    CHECK_EQ(a.clue_found_sfx,   std::string("music/custom_clue.wav"));
    CHECK_EQ(a.weapon_select_sfx, std::string("music/custom_weapon.wav"));
    CHECK_EQ(a.haunt_rise_sfx,   std::string("music/custom_haunt.wav"));
    freshConfig();
}

TEST_CASE("ModConfig: partial audio override only touches the named tracks")
{
    freshConfig();
    // Only rebind two cues; the other five must keep vanilla paths so
    // typos in a hand-edited mods.xml don't silence the whole game.
    const std::string xml =
        "<mods><audio>"
        "<track id=\"title_music\" path=\"music/my_intro.ogg\"/>"
        "<track id=\"ghost_chase\" path=\"music/my_chase.wav\"/>"
        "</audio></mods>";
    REQUIRE(ModConfig::instance().loadFromString(xml));
    const auto& a = ModConfig::instance().audio();
    // Overridden:
    CHECK_EQ(a.title_music,     std::string("music/my_intro.ogg"));
    CHECK_EQ(a.ghost_chase_sfx, std::string("music/my_chase.wav"));
    // Untouched -- still vanilla:
    CHECK_EQ(a.gameplay_music,   std::string("music/start.ogg"));
    CHECK_EQ(a.endgame_music,    std::string("music/gameover.flac"));
    CHECK_EQ(a.lobby_select_sfx, std::string("music/thunder.flac"));
    CHECK_EQ(a.player_hurt_sfx,  std::string("music/hurt.wav"));
    CHECK_EQ(a.player_death_sfx, std::string("music/dead.wav"));
    freshConfig();
}

TEST_CASE("ModConfig: unknown / malformed audio entries are skipped silently")
{
    freshConfig();
    // 1. id="" or path="" -> skip the entire <track>.
    // 2. id="not_a_real_cue" -> recognized id whitelist, so skip.
    // 3. A *valid* track in the same <audio> block still applies, proving
    //    one bad row doesn't poison the rest.
    const std::string xml =
        "<mods><audio>"
        "<track id=\"\"             path=\"music/ignored.ogg\"/>"
        "<track id=\"title_music\"  path=\"\"/>"
        "<track id=\"not_a_cue\"    path=\"music/wat.ogg\"/>"
        "<track id=\"player_hurt\"  path=\"music/real_hit.wav\"/>"
        "</audio></mods>";
    REQUIRE(ModConfig::instance().loadFromString(xml));
    const auto& a = ModConfig::instance().audio();
    // The blank-path title_music override left the default in place
    // (now the empty-string default -- see audio-defaults test).
    CHECK_EQ(a.title_music,     std::string(""));
    // The unknown-id row did nothing.
    CHECK_EQ(a.gameplay_music,  std::string("music/start.ogg"));
    // The valid row still took effect.
    CHECK_EQ(a.player_hurt_sfx, std::string("music/real_hit.wav"));
    freshConfig();
}

TEST_CASE("ModConfig: reset() also clears audio overrides")
{
    freshConfig();
    const std::string xml =
        "<mods><audio>"
        "<track id=\"title_music\" path=\"music/rebind.ogg\"/>"
        "</audio></mods>";
    REQUIRE(ModConfig::instance().loadFromString(xml));
    CHECK_EQ(ModConfig::instance().audio().title_music, std::string("music/rebind.ogg"));

    ModConfig::instance().reset();
    CHECK_EQ(ModConfig::instance().audio().title_music, std::string(""));
}

TEST_CASE("ModConfig: shipped resources/mods.xml audio block parses to vanilla paths")
{
    freshConfig();
    if (!hh::ResourceFS::exists(kVanillaModPath)) {
        // Not all build trees have resources/ available to ResourceFS.
        // Treat as a soft-skip just like the other mods.xml tests do.
        return;
    }
    REQUIRE(ModConfig::instance().loadFromFile(kVanillaModPath));
    const auto& a = ModConfig::instance().audio();
    CHECK_EQ(a.title_music,      std::string(""));
    CHECK_EQ(a.gameplay_music,   std::string("music/start.ogg"));
    CHECK_EQ(a.endgame_music,    std::string("music/gameover.flac"));
    CHECK_EQ(a.lobby_select_sfx, std::string("music/thunder.flac"));
    CHECK_EQ(a.player_hurt_sfx,  std::string("music/hurt.wav"));
    CHECK_EQ(a.player_death_sfx, std::string("music/dead.wav"));
    CHECK_EQ(a.ghost_chase_sfx,  std::string("music/chase.wav"));
    freshConfig();
}

TEST_MAIN()
