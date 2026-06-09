#ifndef HH_ENGINE_RESOURCE_FS_HPP
#define HH_ENGINE_RESOURCE_FS_HPP

#include <cstddef>
#include <string>
#include <vector>

// ResourceFS
// ----------
//
// Unified "give me the bytes of a resource file" API. Hides the
// difference between two backends:
//
//   * Disk (default development builds): reads through std::ifstream.
//     The on-disk location is composed by Paths::resolveForBackend()
//     so callers never have to know about "../resources/...".
//
//   * Embedded (HH_EMBED_RESOURCES=ON, used by the redistributable
//     standalone build): reads from Win32 RT_RCDATA resources that the
//     resource compiler baked into HH.exe at link time. Internally the
//     lookup goes through hh::embedded::find() and the bytes live in
//     the .exe's .rsrc section (zero-copy at the platform layer).
//
// The same call sites in game code work unchanged: every loader
// (sf::Texture, sf::Font, sf::Shader, sf::Image, ModConfig, ClueReader)
// passes a logical key like "fonts/Arial.ttf" -- the same string they
// get back from Paths::resource(...) -- and ResourceFS handles the
// rest. (AudioPlayer talks to miniaudio directly because miniaudio
// expects to do its own I/O, but it routes paths through the same
// Paths::resolveForBackend helper for symmetry.)

namespace hh {

class ResourceFS {
public:
    // Copy the resource bytes into ``out``. Returns false if the
    // resource cannot be located (logged to stderr in embedded mode so
    // a missing bake is visible).
    //
    // ``key`` is a logical, POSIX-style relative path under resources/
    // (e.g. "items.xml" or "fonts/Arial.ttf"). This is the same string
    // that Paths::resource(...) returns.
    static bool readAll(const std::string& key, std::vector<char>& out);

    // True if a resource at ``key`` is available (either on disk or
    // embedded). Cheap probe -- doesn't read the bytes.
    static bool exists(const std::string& key);
};

}  // namespace hh

#endif  // HH_ENGINE_RESOURCE_FS_HPP

