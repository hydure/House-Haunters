#ifndef HH_ENGINE_EMBEDDED_RESOURCES_HPP
#define HH_ENGINE_EMBEDDED_RESOURCES_HPP

#include <cstddef>

// EmbeddedResources.hpp
// ---------------------
//
// Internal interface between a generated resource table and the runtime
// ResourceFS dispatcher. Windows maps IDs to RT_RCDATA; macOS and Linux use
// compiler-neutral byte arrays. Only compiled when HH_EMBED_RESOURCES is ON.
//
// This header should NOT be included from game code -- call
// hh::ResourceFS::readAll() instead. The only direct consumer is
// AudioPlayer.cpp, which walks the table at engine init to register
// every audio file with miniaudio's resource manager.

namespace hh { namespace embedded {

// One row in the generated lookup table. ``path`` is a stable, plain
// C string literal (resides in the executable's read-only data section)
// holding a POSIX-style relative path from the project's resources/
// directory, e.g. ``"fonts/Underdog-Regular.ttf"``.
struct Entry {
    const char* path;
    int         id;     // Win32 RT_RCDATA ID; zero on portable backends.
};

// Returns the generated table (sorted ascending by ``path``) and its
// length. Call sites do a linear scan -- the table has on the order of
// 100 entries and lookups happen during one-time asset loads, so a hash
// map would be overkill.
const Entry* table()      noexcept;
std::size_t  table_size() noexcept;

// Resolve a path to a (ptr, size) pair pointing at the resource bytes
// embedded in the .exe. ``data`` is a pointer into the OS-mapped image
// of the executable, valid for the lifetime of the process. Returns
// false on miss; ``data`` and ``size`` are left untouched on failure.
bool find(const char* posix_relative_path,
          const void** data,
          std::size_t* size) noexcept;

}}  // namespace hh::embedded

#endif  // HH_ENGINE_EMBEDDED_RESOURCES_HPP
