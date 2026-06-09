// ResourceFS tests
// ----------------
//
// Pin the contract of the unified resource read path. ResourceFS is on
// the load path for fonts/textures/shaders/mods/clues/audio, so a
// regression here breaks "the game won't start" -- worth a dedicated
// suite that's cheap to run.
//
// Both build modes are covered by the same tests because both backends
// honor the same logical-key contract:
//   * Disk build:     ResourceFS reads through Paths::resolveForBackend
//                     -> std::ifstream against "../resources/<key>".
//   * Embedded build: ResourceFS reads from the table baked into
//                     HH.exe's .rsrc section.
// In either case readAll("items.xml") must return the bytes of
// resources/items.xml and exists("items.xml") must report true.

#include "test_harness.hpp"
#include "engine/ResourceFS.hpp"

#include <cstring>
#include <string>
#include <vector>

namespace {
// Pick assets we know exist in resources/ (anchored by the generator's
// blocklist + the vanilla content). Keep this list small -- a missing
// asset here is a meaningful regression, but we don't need a sweep.
const char* kKnownTextKey   = "items.xml";   // ASCII XML
const char* kKnownModXml    = "mods.xml";    // ASCII XML
const char* kKnownFontKey   = "fonts/Underdog-Regular.ttf"; // binary, title screen font
const char* kMissingKey     = "definitely/not/a/real/key.bin";
}  // namespace

TEST_CASE("ResourceFS::exists reports true for shipped assets")
{
    CHECK(hh::ResourceFS::exists(kKnownTextKey));
    CHECK(hh::ResourceFS::exists(kKnownModXml));
    CHECK(hh::ResourceFS::exists(kKnownFontKey));
}

TEST_CASE("ResourceFS::exists reports false for unknown keys")
{
    CHECK(!hh::ResourceFS::exists(kMissingKey));
    CHECK(!hh::ResourceFS::exists(""));  // empty key isn't a resource
}

TEST_CASE("ResourceFS::readAll returns non-empty bytes for a known text asset")
{
    std::vector<char> bytes;
    REQUIRE(hh::ResourceFS::readAll(kKnownTextKey, bytes));
    CHECK(!bytes.empty());
    // items.xml is a well-formed XML document; first non-whitespace
    // bytes are "<?xml" (or "<items" if the prolog were stripped). The
    // looser check below tolerates either, plus a UTF-8 BOM, so a
    // future XML reformat doesn't break the test.
    REQUIRE(bytes.size() >= 1);
    // Skip a possible UTF-8 BOM.
    std::size_t i = 0;
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        i = 3;
    }
    CHECK_EQ(bytes[i], '<');
}

TEST_CASE("ResourceFS::readAll returns non-empty bytes for a binary asset")
{
    std::vector<char> bytes;
    REQUIRE(hh::ResourceFS::readAll(kKnownFontKey, bytes));
    // A TTF is at least a 12-byte offset table -- a font smaller than
    // that wouldn't load anywhere else either. Cheap sanity check.
    CHECK(bytes.size() >= 12);
}

TEST_CASE("ResourceFS::readAll fails cleanly on missing keys")
{
    // Pre-fill the output buffer so we can verify readAll resets it on
    // failure -- callers should never see stale bytes after a miss.
    std::vector<char> bytes(64, 'x');
    CHECK(!hh::ResourceFS::readAll(kMissingKey, bytes));
    CHECK(bytes.empty());
}

TEST_CASE("ResourceFS::readAll overwrites prior contents on success")
{
    // Seed with sentinel bytes; the previous test established that
    // failure clears the vector, but success should also overwrite
    // rather than append.
    std::vector<char> bytes(16, '!');
    REQUIRE(hh::ResourceFS::readAll(kKnownTextKey, bytes));
    CHECK(!bytes.empty());
    CHECK(bytes[0] != '!');  // first byte is XML, not the sentinel
}

TEST_CASE("ResourceFS::readAll is idempotent across repeated calls")
{
    std::vector<char> first;
    std::vector<char> second;
    REQUIRE(hh::ResourceFS::readAll(kKnownTextKey, first));
    REQUIRE(hh::ResourceFS::readAll(kKnownTextKey, second));
    CHECK_EQ(first.size(), second.size());
    CHECK(std::memcmp(first.data(), second.data(), first.size()) == 0);
}

TEST_MAIN()
