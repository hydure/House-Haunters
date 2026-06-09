#ifndef ENGINE_ENV_HPP
#define ENGINE_ENV_HPP

#include <string>

////////////////////////////////////
// Env.hpp
//
// Tiny portable env-var reader. Exists so every call site can use a
// single helper instead of std::getenv (which MSVC's CRT marks
// deprecated under /W4 in favor of the non-portable _dupenv_s). The
// helper uses the platform-appropriate API under the hood:
//   * MSVC  -> _dupenv_s   (does its own allocation; we free the buffer)
//   * other -> std::getenv (no _s variant available)
//
// Always returns a std::string by value -- empty string means "unset".
// Callers that need to distinguish "unset" from "set to empty" should
// use Env::tryGet().
////////////////////////////////////
namespace Env {

// Returns the env var value, or empty string if unset / empty.
std::string get(const char* name);

// Returns true and writes the env var value into *out when the variable
// is set (even to an empty string). Returns false when the variable is
// unset; *out is left unchanged.
bool tryGet(const char* name, std::string* out);

// Convenience: treat the env var as a truthy flag. Returns true when
// the value (lowercased) is one of "1", "true", "on", "yes".
bool asBool(const char* name);

} // namespace Env

#endif
