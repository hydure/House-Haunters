// Paths tests
// -----------
//
// Two helpers, two distinct contracts:
//
//   Paths::resource(rel) is the call-site convention: takes a logical
//   resource key (POSIX-relative path under resources/) and returns it
//   unchanged. The function exists so a grep for "Paths::resource("
//   surfaces every asset lookup in the game; today it's identity, but
//   future overlay / mod-priority work has a hook.
//
//   Paths::resolveForBackend(key) is the one place that knows how to
//   translate a logical key into something the OS or miniaudio can
//   actually open. Disk builds prepend "../resources/" (the binary
//   launches from build*/ which sits next to resources/); embedded
//   builds (HH_EMBED_RESOURCES) return the key unchanged because the
//   embedded resource table and miniaudio's registry both index by the
//   bare logical path.

#include "test_harness.hpp"
#include "engine/Paths.hpp"

TEST_CASE("Paths::resource returns the logical key unchanged")
{
    CHECK_EQ(Paths::resource("items.xml"), std::string("items.xml"));
    CHECK_EQ(Paths::resource("fonts/Arial.ttf"), std::string("fonts/Arial.ttf"));
    CHECK_EQ(Paths::resource("room/bathroom.tmx"), std::string("room/bathroom.tmx"));
}

TEST_CASE("Paths::resource handles empty input")
{
    // Empty in, empty out. Documents the trivial identity contract so
    // anyone tightening this (e.g. asserting non-empty) sees the test
    // fail and considers the change.
    CHECK_EQ(Paths::resource(""), std::string(""));
}

TEST_CASE("Paths::resolveForBackend produces a backend-ready path")
{
    // The actual returned string depends on HH_EMBED_RESOURCES. We
    // verify the right branch was compiled in by comparing against
    // what the corresponding ResourceFS backend will eat.
#if defined(HH_EMBED_RESOURCES)
    // Embedded: identity. The embedded table keys are bare logical
    // paths, and miniaudio's registry is populated with the same.
    CHECK_EQ(Paths::resolveForBackend("items.xml"), std::string("items.xml"));
    CHECK_EQ(Paths::resolveForBackend("fonts/Arial.ttf"),
             std::string("fonts/Arial.ttf"));
    CHECK_EQ(Paths::resolveForBackend(""), std::string(""));
#else
    // Disk: prepend the relative-to-launch-dir prefix.
    CHECK_EQ(Paths::resolveForBackend("items.xml"),
             std::string("../resources/items.xml"));
    CHECK_EQ(Paths::resolveForBackend("fonts/Arial.ttf"),
             std::string("../resources/fonts/Arial.ttf"));
    CHECK_EQ(Paths::resolveForBackend(""), std::string("../resources/"));
#endif
}

TEST_MAIN()
