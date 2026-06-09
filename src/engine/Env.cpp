#include "engine/Env.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace Env {

bool tryGet(const char* name, std::string* out)
{
    if (!name) return false;
#if defined(_MSC_VER)
    // Microsoft CRT: _dupenv_s allocates a buffer the caller owns. The
    // _s variant is what /W4 wants us to use instead of getenv() (which
    // shows up as a C4996 deprecation warning).
    char*  buf = nullptr;
    size_t len = 0;
    if (_dupenv_s(&buf, &len, name) != 0) return false;
    if (buf == nullptr) return false; // variable not present
    if (out) *out = std::string(buf);
    std::free(buf);
    return true;
#else
    const char* v = std::getenv(name);
    if (!v) return false;
    if (out) *out = std::string(v);
    return true;
#endif
}

std::string get(const char* name)
{
    std::string s;
    return tryGet(name, &s) ? s : std::string();
}

bool asBool(const char* name)
{
    std::string v;
    if (!tryGet(name, &v)) return false;
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return v == "1" || v == "true" || v == "on" || v == "yes";
}

} // namespace Env
