#ifndef PATHS_HPP
#define PATHS_HPP

#include <string>

////////////////////////////////
// Paths.hpp
//
// Logical resource keys + the one helper that knows how to turn a key
// into something the operating system (or miniaudio's resource manager)
// can actually open.
//
// Game code only ever sees the LOGICAL key: a POSIX-style relative
// path under resources/, e.g. "sound/foo.wav" or "fonts/Arial.ttf".
// Both ResourceFS and AudioPlayer accept these keys. Neither call
// sites nor backends need to know whether the asset lives on disk or
// inside HH.exe's .rsrc section.
//
// Only one place translates the key into an actual filesystem path:
// Paths::resolveForBackend() below. Disk-mode it prepends
// "../resources/" (because the binary launches from build*/, one folder
// below the resources/ tree). Embedded-mode it returns the key
// unchanged (the embedded resource manager / miniaudio registry lookup
// key IS the bare logical path).
////////////////////////////////

namespace Paths
{
    // Logical resource key. Today this is the identity function -- it
    // exists as a stable call-site convention so a grep for
    // Paths::resource(...) finds every resource lookup, and so future
    // re-rooting (per-mod overlays, language packs, etc.) has one
    // place to hook.
    inline std::string resource(const std::string& rel)
    {
        return rel;
    }

    // Translate a logical key into the string a backend needs to open
    // the bytes. Used by ResourceFS (for std::ifstream paths) and by
    // AudioPlayer (for miniaudio's resource-manager registry and
    // ma_sound_init_from_file calls). Game code should NOT call this --
    // ask ResourceFS / AudioPlayer for the asset by its logical key.
    inline std::string resolveForBackend(const std::string& key)
    {
#if defined(HH_EMBED_RESOURCES)
        // The embedded resource manager and miniaudio registry both
        // index by the bare logical path, so no translation needed.
        return key;
#else
        // Disk layout: the executable runs out of build*/ and the
        // resources/ tree is its sibling.
        return "../resources/" + key;
#endif
    }
}

#endif

