#include "engine/ResourceManager.hpp"
#include "engine/ResourceFS.hpp"

#include <iostream>
#include <utility>
#include <vector>

std::map<std::string, ResourceManager::FontEntry> ResourceManager::fonts_cache;
std::map<std::string, sf::Texture> ResourceManager::textures_cache;

namespace {

// Texture loader: SFML's sf::Texture::loadFromMemory copies into GPU
// memory immediately, so we can let the byte buffer go out of scope.
// Fonts are different (see getFont below) and don't use this helper.
bool loadTextureBytes(sf::Texture& dst, const std::string& key, const char* kind)
{
    std::vector<char> bytes;
    if (!hh::ResourceFS::readAll(key, bytes)) {
        std::cout << kind << ' ' << key << " not found or could not be loaded!" << std::endl;
        return false;
    }
    if (bytes.empty()) {
        std::cout << kind << ' ' << key << " was empty!" << std::endl;
        return false;
    }
    if (!dst.loadFromMemory(bytes.data(), bytes.size())) {
        std::cout << kind << ' ' << key << " bytes could not be decoded!" << std::endl;
        return false;
    }
    return true;
}

}  // namespace

sf::Font* ResourceManager::getFont(std::string name)
{
    auto it = fonts_cache.find(name);
    if (it != fonts_cache.end()) {
        return &it->second.font;
    }

    // Insert an empty entry first so the bytes vector lives in the
    // cache (i.e. at a stable address). sf::Font::loadFromMemory
    // borrows that pointer for the lifetime of the Font; if the bytes
    // were a local vector they'd dangle the moment this function
    // returned and the first draw call would segfault.
    auto inserted = fonts_cache.emplace(std::move(name), FontEntry{});
    FontEntry& entry = inserted.first->second;
    const std::string& key = inserted.first->first;

    if (!hh::ResourceFS::readAll(key, entry.bytes)) {
        std::cout << "Font " << key << " not found or could not be loaded!" << std::endl;
        return &entry.font;
    }
    if (entry.bytes.empty()) {
        std::cout << "Font " << key << " was empty!" << std::endl;
        return &entry.font;
    }
    if (!entry.font.loadFromMemory(entry.bytes.data(), entry.bytes.size())) {
        std::cout << "Font " << key << " bytes could not be parsed!" << std::endl;
    }
    return &entry.font;
}

sf::Texture* ResourceManager::getTexture(std::string name)
{
    auto it = textures_cache.find(name);
    if (it != textures_cache.end()) {
        return &it->second;
    }
    sf::Texture t;
    // Cache the empty texture even on failure so repeated lookups don't
    // keep retrying the same failed I/O. SFML emits its own diagnostic
    // line via stderr (e.g. "internal size is too high" when a PNG
    // exceeds the host GPU's max texture size).
    loadTextureBytes(t, name, "Texture");
    auto inserted = textures_cache.emplace(std::move(name), std::move(t));
    return &inserted.first->second;
}

void ResourceManager::clear()
{
    fonts_cache.clear();
    textures_cache.clear();
}
