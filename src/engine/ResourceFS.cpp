// ResourceFS.cpp
// --------------
//
// Two backends compiled in via the HH_EMBED_RESOURCES preprocessor
// flag. The disk backend uses std::ifstream against the path that
// Paths::resolveForBackend produces; the embedded backend forwards
// to hh::embedded::find(), which on Windows resolves to
// FindResource/LockResource against HH.exe's .rsrc section.
//
// No internal byte cache: every loader we hand off to
// (sf::Texture::loadFromMemory, rapidxml, shader compile, etc.)
// immediately copies the bytes into its own representation, so caching
// here would just hold a duplicate of memory the OS file cache or the
// .exe mapping already keeps for us.

#include "engine/ResourceFS.hpp"
#include "engine/Paths.hpp"

#include <cstdio>

#if defined(HH_EMBED_RESOURCES)
#  include "engine/EmbeddedResources.hpp"
#  include <cstring>
#else
#  include <fstream>
#  include <ios>
#endif

namespace hh {

bool ResourceFS::readAll(const std::string& key, std::vector<char>& out)
{
    out.clear();
#if defined(HH_EMBED_RESOURCES)
    const void*  data = nullptr;
    std::size_t  size = 0;
    if (!embedded::find(key.c_str(), &data, &size)) {
        // Loudly: a missing key in embedded mode means we baked an
        // incomplete bundle, which is exactly the kind of bug we want
        // to surface during testing rather than mask with a fallback.
        std::fprintf(stderr,
            "[ResourceFS] embedded miss for \"%s\"\n", key.c_str());
        return false;
    }
    out.resize(size);
    if (size > 0 && data) {
        std::memcpy(out.data(), data, size);
    }
    return true;
#else
    const std::string path = Paths::resolveForBackend(key);
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.good()) return false;

    const std::streampos sz = f.tellg();
    if (sz < 0) return false;
    f.seekg(0, std::ios::beg);

    out.resize(static_cast<std::size_t>(sz));
    if (!out.empty()) {
        f.read(out.data(), static_cast<std::streamsize>(out.size()));
        if (!f) {
            out.clear();
            return false;
        }
    }
    return true;
#endif
}

bool ResourceFS::exists(const std::string& key)
{
#if defined(HH_EMBED_RESOURCES)
    const void*  d = nullptr;
    std::size_t  s = 0;
    return embedded::find(key.c_str(), &d, &s);
#else
    std::ifstream f(Paths::resolveForBackend(key), std::ios::binary);
    return f.good();
#endif
}

}  // namespace hh
