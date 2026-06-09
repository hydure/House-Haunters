// EmbeddedResources_Win32.cpp
// ---------------------------
//
// Implements hh::embedded::find() on Windows by locating an RT_RCDATA
// resource that was linked into HH.exe from the generated
// embedded_resources.rc script (see tools/generate_embedded_resources.py).
//
// The lookup is a linear scan over the generated table. The table is
// O(100) entries and the scan only runs once per asset (results are
// then cached by the higher-level ResourceManager / AudioPlayer engine),
// so a hash table would be over-engineering.
//
// Compiled only when HH_EMBED_RESOURCES is ON. Generator-emitted
// embedded_resources_table.cpp pairs up with this file to provide the
// path -> ID mapping.

#include "engine/EmbeddedResources.hpp"

// The entire body is gated: in non-embedded builds this TU produces an
// empty object file. We keep the file in the SRC glob (so the build
// graph is uniform across configurations) but emit no symbols, which
// avoids needing a generated table to satisfy the linker.
#if defined(HH_EMBED_RESOURCES) && defined(_WIN32)

#include <cstring>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace hh { namespace embedded {

namespace {

// HINSTANCE of the currently-loaded module containing the embedded
// resources. Because we link the .rc into the main executable (not a
// DLL), passing nullptr to FindResource/LoadResource also works on
// modern Windows, but being explicit shields us from surprises if this
// file is ever pulled into a shared library build.
HMODULE module_handle() noexcept
{
    return ::GetModuleHandleW(nullptr);
}

}  // namespace

bool find(const char* posix_relative_path,
          const void** data,
          std::size_t* size) noexcept
{
    if (!posix_relative_path || !data || !size) return false;

    const Entry* tbl = table();
    const std::size_t n = table_size();

    int found_id = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (std::strcmp(tbl[i].path, posix_relative_path) == 0) {
            found_id = tbl[i].id;
            break;
        }
    }
    if (found_id == 0) return false;

    HMODULE mod = module_handle();
    // MAKEINTRESOURCEA returns the integer cast as LPCSTR -- the standard
    // Win32 idiom for resource IDs.
    HRSRC res = ::FindResourceA(mod, MAKEINTRESOURCEA(found_id), RT_RCDATA);
    if (!res) return false;

    DWORD sz = ::SizeofResource(mod, res);
    if (sz == 0) return false;

    HGLOBAL hData = ::LoadResource(mod, res);
    if (!hData) return false;

    void* ptr = ::LockResource(hData);
    if (!ptr) return false;

    *data = ptr;
    *size = static_cast<std::size_t>(sz);
    return true;
}

}}  // namespace hh::embedded

#endif  // HH_EMBED_RESOURCES && _WIN32
