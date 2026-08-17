// ModConfig.cpp
//
// rapidxml-backed loader for resources/mods.xml.
//
// Design notes:
//   * rapidxml's parse<0> stores pointers into the source buffer, so the
//     buffer (and any std::string we copy it into) MUST outlive the
//     xml_document. We keep an instance-scoped std::string for the file
//     contents -- same pattern ClueReader uses.
//   * Every "read attribute / read child node" call is wrapped in a
//     null-check + fallback so a partial mods.xml just leaves the missing
//     value at its default. Modders can therefore override one field at a
//     time without re-declaring the whole config.
//   * Parsing exceptions thrown by rapidxml are caught and converted into
//     a `false` return + lastError_ string. The game keeps running with
//     defaults rather than dying on a typo in a mod file.

#include "engine/ModConfig.hpp"
#include "engine/ResourceFS.hpp"

#include "rapidxml/rapidxml.hpp"

#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

namespace {

// ---- rapidxml attribute helpers -----------------------------------------
// All "read X with fallback" goo lives here so the parse functions below
// stay short and read top-to-bottom in the same order as mods.xml itself.

const char* attrValue(rapidxml::xml_node<>* n, const char* name)
{
    if (!n) return nullptr;
    auto* a = n->first_attribute(name);
    return a ? a->value() : nullptr;
}

std::string attrString(rapidxml::xml_node<>* n, const char* name,
                       const std::string& fallback)
{
    const char* v = attrValue(n, name);
    return v ? std::string(v) : fallback;
}

int attrInt(rapidxml::xml_node<>* n, const char* name, int fallback)
{
    const char* v = attrValue(n, name);
    if (!v || !*v) return fallback;
    // strtol over std::stoi: stoi throws on bad input which we'd have to
    // catch anyway. strtol just signals via endptr.
    char* end = nullptr;
    long parsed = std::strtol(v, &end, 10);
    return (end == v) ? fallback : static_cast<int>(parsed);
}

float attrFloat(rapidxml::xml_node<>* n, const char* name, float fallback)
{
    const char* v = attrValue(n, name);
    if (!v || !*v) return fallback;
    char* end = nullptr;
    float parsed = std::strtof(v, &end);
    return (end == v) ? fallback : parsed;
}

double attrDouble(rapidxml::xml_node<>* n, const char* name, double fallback)
{
    const char* v = attrValue(n, name);
    if (!v || !*v) return fallback;
    char* end = nullptr;
    double parsed = std::strtod(v, &end);
    return (end == v) ? fallback : parsed;
}

// Parse a comma-separated list of ints ("1,2,1,0"). Whitespace is
// tolerated. On any parse failure we return the fallback list unchanged so
// the villain still has a usable walk cycle.
std::vector<int> splitInts(const char* csv, const std::vector<int>& fallback)
{
    if (!csv || !*csv) return fallback;
    std::vector<int> out;
    std::string token;
    auto flush = [&]() {
        if (token.empty()) return;
        char* end = nullptr;
        long parsed = std::strtol(token.c_str(), &end, 10);
        if (end != token.c_str()) {
            out.push_back(static_cast<int>(parsed));
        }
        token.clear();
    };
    for (const char* p = csv; *p; ++p) {
        if (*p == ',') {
            flush();
        }
        else if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            // Ignore internal whitespace -- "1, 2, 1, 0" works too.
        }
        else {
            token.push_back(*p);
        }
    }
    flush();
    return out.empty() ? fallback : out;
}

// Map a character id string ("BRO" / "SIS" / "DAD" / "MOM" or "0".."3")
// to the canonical enum index. Returns -1 on unknown ids so the parser
// can skip an entry rather than overwriting the wrong character's stats.
int characterIdToIndex(const std::string& id)
{
    if (id == "BRO" || id == "bro" || id == "0") return 0;
    if (id == "SIS" || id == "sis" || id == "1") return 1;
    if (id == "DAD" || id == "dad" || id == "2") return 2;
    if (id == "MOM" || id == "mom" || id == "3") return 3;
    return -1;
}

} // namespace

// ---- ModConfig ----------------------------------------------------------

ModConfig& ModConfig::instance()
{
    // Magic-statics give us thread-safe lazy init without a mutex (C++11
    // guarantees this). The defaults are populated in the constructor so
    // the very first instance() call already returns a usable object even
    // if loadFromFile() is never called.
    static ModConfig s_instance;
    return s_instance;
}

ModConfig::ModConfig()
{
    applyDefaults();
}

void ModConfig::reset()
{
    applyDefaults();
    loaded_ = false;
    lastError_.clear();
}

void ModConfig::applyDefaults()
{
    // Per-character defaults reproduce the original Character::init switch
    // table EXACTLY. The (id, sprite_location, hp, speed_mul, portrait)
    // tuples below match vanilla; do not change without also updating the
    // tests in tests/test_mod_config.cpp.
    //
    //   index 0 = BRO -> sprite slot 2 (mod=24), hp=3, x1.0, portrait 0
    //   index 1 = SIS -> sprite slot 1 (mod=3),  hp=3, x1.0, portrait 2
    //   index 2 = DAD -> sprite slot 3 (mod=27), hp=5, x1.0, portrait 4
    //   index 3 = MOM -> sprite slot 0 (mod=0),  hp=3, x0.85, portrait 6
    characters_[0] = CharacterMod{}; // BRO
    characters_[0].portrait_index   = 0;
    characters_[0].health           = 3;
    characters_[0].max_health       = 3;
    characters_[0].speed_multiplier = 1.0;
    characters_[0].sprite_location  = 2;

    characters_[1] = CharacterMod{}; // SIS
    characters_[1].portrait_index   = 2;
    characters_[1].health           = 3;
    characters_[1].max_health       = 3;
    characters_[1].speed_multiplier = 1.0;
    characters_[1].sprite_location  = 1;

    characters_[2] = CharacterMod{}; // DAD
    characters_[2].portrait_index   = 4;
    characters_[2].health           = 5;
    characters_[2].max_health       = 5;
    characters_[2].speed_multiplier = 1.0;
    characters_[2].sprite_location  = 3;

    characters_[3] = CharacterMod{}; // MOM
    characters_[3].portrait_index   = 6;
    characters_[3].health           = 3;
    characters_[3].max_health       = 3;
    characters_[3].speed_multiplier = 0.85;
    characters_[3].sprite_location  = 0;

    villain_ = VillainMod{};
    screen_  = ScreenMod{};
    audio_   = AudioMod{};
}

bool ModConfig::loadFromFile(const std::string& path)
{
    // Route through ResourceFS so loading works for both disk-backed
    // dev builds (std::ifstream under the hood) and the standalone
    // embedded build (bytes pulled from the .exe's RT_RCDATA section).
    // We pass the bytes to loadFromString to share parser logic with
    // unit tests, which build the XML in-memory.
    std::vector<char> bytes;
    if (!hh::ResourceFS::readAll(path, bytes)) {
        lastError_ = "mods.xml not found at: " + path;
        loaded_ = false;
        return false;
    }
    return loadFromString(std::string(bytes.begin(), bytes.end()));
}

bool ModConfig::loadFromString(const std::string& xml)
{
    // Start from a clean slate so partial overrides reset to defaults
    // first. A consumer that wants to layer two configs would need to
    // build the merged XML themselves.
    applyDefaults();
    lastError_.clear();

    if (xml.empty()) {
        lastError_ = "empty mod config string";
        loaded_ = false;
        return false;
    }

    // rapidxml mutates its source buffer in-place (it inserts nulls to
    // terminate tag/attribute values). We need a non-const copy that
    // outlives the parser; a stack-local std::vector<char> is enough
    // because we extract everything we care about before this function
    // returns.
    std::vector<char> mutableBuf(xml.begin(), xml.end());
    mutableBuf.push_back('\0');
    return parseBuffer(mutableBuf.data());
}

bool ModConfig::parseBuffer(char* mutableBuffer)
{
    using namespace rapidxml;
    xml_document<> doc;
    try {
        doc.parse<0>(mutableBuffer);
    }
    catch (const std::exception& ex) {
        lastError_ = std::string("rapidxml parse error: ") + ex.what();
        loaded_ = false;
        return false;
    }

    xml_node<>* root = doc.first_node("mods");
    if (!root) {
        lastError_ = "missing <mods> root node";
        loaded_ = false;
        return false;
    }

    // ---- <screen> -------------------------------------------------------
    if (auto* s = root->first_node("screen")) {
        if (auto* t = s->first_node("title")) {
            screen_.title_text = attrString(t, "text",  screen_.title_text);
            screen_.title_font = attrString(t, "font",  screen_.title_font);
            screen_.title_size = attrInt   (t, "size",  screen_.title_size);
            screen_.title_x    = attrInt   (t, "x",     screen_.title_x);
            screen_.title_y    = attrInt   (t, "y",     screen_.title_y);
        }
        if (auto* b = s->first_node("background")) {
            // Clamp to 0..255 so a stray "300" or "-1" can't smuggle a
            // garbage value into sf::Uint8.
            auto clamp255 = [](int v) {
                if (v < 0)   return 0;
                if (v > 255) return 255;
                return v;
            };
            screen_.background_color.r = static_cast<sf::Uint8>(clamp255(attrInt(b, "r", screen_.background_color.r)));
            screen_.background_color.g = static_cast<sf::Uint8>(clamp255(attrInt(b, "g", screen_.background_color.g)));
            screen_.background_color.b = static_cast<sf::Uint8>(clamp255(attrInt(b, "b", screen_.background_color.b)));
        }
        if (auto* p = s->first_node("portraits")) {
            screen_.portrait_atlas      = attrString(p, "atlas",       screen_.portrait_atlas);
            screen_.portrait_tile_w     = attrInt   (p, "tile_width",  screen_.portrait_tile_w);
            screen_.portrait_tile_h     = attrInt   (p, "tile_height", screen_.portrait_tile_h);
            screen_.portrait_atlas_cols = attrInt   (p, "columns",     screen_.portrait_atlas_cols);
            screen_.portrait_scale      = attrFloat (p, "scale",       screen_.portrait_scale);
            if (screen_.portrait_atlas_cols < 1) screen_.portrait_atlas_cols = 1;
        }
        if (auto* L = s->first_node("layout")) {
            screen_.layout_start_x   = attrFloat(L, "start_x",   screen_.layout_start_x);
            screen_.layout_start_y   = attrFloat(L, "start_y",   screen_.layout_start_y);
            screen_.layout_x_spacing = attrFloat(L, "x_spacing", screen_.layout_x_spacing);
        }
    }

    // ---- <characters> ---------------------------------------------------
    if (auto* cs = root->first_node("characters")) {
        for (xml_node<>* c = cs->first_node("character"); c; c = c->next_sibling("character")) {
            const std::string id = attrString(c, "id", "");
            const int idx = characterIdToIndex(id);
            if (idx < 0) continue; // unknown id -> skip silently
            CharacterMod& cm = characters_[static_cast<size_t>(idx)];
            cm.portrait_index   = attrInt   (c, "portrait_index",   cm.portrait_index);
            cm.health           = attrInt   (c, "hp",               cm.health);
            cm.max_health       = attrInt   (c, "max_hp",           cm.health);
            cm.speed_multiplier = attrDouble(c, "speed_multiplier", cm.speed_multiplier);
            cm.sprite_location  = attrInt   (c, "sprite_location",  cm.sprite_location);
            cm.sprite_sheet_path= attrString(c, "sprite_sheet",     cm.sprite_sheet_path);
        }
    }

    // ---- <villain> ------------------------------------------------------
    if (auto* v = root->first_node("villain")) {
        villain_.sprite_sheet_path = attrString(v, "sprite_sheet", villain_.sprite_sheet_path);
        villain_.frame_width       = attrInt   (v, "frame_width",  villain_.frame_width);
        villain_.frame_height      = attrInt   (v, "frame_height", villain_.frame_height);
        villain_.health            = attrInt   (v, "health",       villain_.health);
        for (xml_node<>* w = v->first_node("walk"); w; w = w->next_sibling("walk")) {
            const std::string dir = attrString(w, "direction", "");
            const char* csv = attrValue(w, "frames");
            if      (dir == "down")  villain_.walk_down  = splitInts(csv, villain_.walk_down);
            else if (dir == "left")  villain_.walk_left  = splitInts(csv, villain_.walk_left);
            else if (dir == "right") villain_.walk_right = splitInts(csv, villain_.walk_right);
            else if (dir == "up")    villain_.walk_up    = splitInts(csv, villain_.walk_up);
        }
    }

    // ---- <audio> --------------------------------------------------------
    // Each <track id="..." path="..."/> entry overrides one cue. Unknown
    // ids are silently skipped so a future renaming of cues stays backwards
    // compatible (the old name simply does nothing).
    if (auto* a = root->first_node("audio")) {
        for (xml_node<>* t = a->first_node("track"); t; t = t->next_sibling("track")) {
            const std::string id   = attrString(t, "id",   "");
            const std::string path = attrString(t, "path", "");
            if (id.empty() || path.empty()) continue;
            if      (id == "title_music")     audio_.title_music     = path;
            else if (id == "gameplay_music")  audio_.gameplay_music  = path;
            else if (id == "endgame_music")   audio_.endgame_music   = path;
            else if (id == "lobby_select")    audio_.lobby_select_sfx = path;
            else if (id == "player_hurt")     audio_.player_hurt_sfx  = path;
            else if (id == "player_death")    audio_.player_death_sfx = path;
            else if (id == "ghost_chase")     audio_.ghost_chase_sfx  = path;
            else if (id == "clue_found")      audio_.clue_found_sfx    = path;
            else if (id == "weapon_select")   audio_.weapon_select_sfx = path;
            else if (id == "haunt_rise")      audio_.haunt_rise_sfx    = path;
            // unknown id -> skip silently (no surprise rebinds).
        }
    }

    loaded_ = true;
    return true;
}

const ModConfig::CharacterMod& ModConfig::character(int index) const
{
    if (index < 0 || index >= static_cast<int>(characters_.size())) {
        return characters_[0];
    }
    return characters_[static_cast<size_t>(index)];
}
