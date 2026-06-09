#ifndef RESOURCE_MANAGER_HPP
#define RESOURCE_MANAGER_HPP

#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include <vector>

class ResourceManager
{
public:
    static sf::Font* getFont(std::string name);
    static sf::Texture* getTexture(std::string name);

    // Drops every cached font / texture. Provided so the game can
    // reclaim memory between sessions without restarting the process.
    // Audio cues are owned by hh::Sound / hh::Music directly and
    // managed through engine/AudioPlayer.hpp, so they are NOT cleared
    // by this call (call hh::AudioPlayer::shutdown() for those).
    //
    // WARNING: invalidates every pointer previously returned by
    // getFont/getTexture. Call only when no game objects hold those
    // pointers (i.e. between sessions, not mid-frame).
    static void clear();
private:
    // sf::Font::loadFromMemory keeps a borrowed pointer into the byte
    // buffer for the lifetime of the Font (see SFML 2.6 docs: "the
    // contents of data have to remain valid as long as the font is
    // used"). We bundle the bytes alongside the Font so the buffer
    // outlives the Font. Textures don't need this because
    // sf::Texture::loadFromMemory copies into GPU memory.
    struct FontEntry {
        std::vector<char> bytes;
        sf::Font          font;
    };
    static std::map<std::string, FontEntry> fonts_cache;
    static std::map<std::string, sf::Texture> textures_cache;
};

#endif