// ClueReader: parse resources/items.xml and surface the per-clue strings.
//
// The reader is exercised at game start so a regression here is a "won't
// boot" bug. We feed it the same logical resource key the game uses
// (Paths::resource("items.xml") == "items.xml") and let ResourceFS pick
// the disk vs embedded backend.

#include "test_harness.hpp"
#include "engine/ClueReader.hpp"
#include "engine/Random.hpp"
#include "engine/ResourceFS.hpp"

#include <string>

namespace {
// Logical resource key, identical to what the game launcher passes:
//   ClueReader::readFile(Paths::resource("items.xml"))
// Tests pass this same string straight into the reader and into the
// existence probe so they stay in lockstep with production.
std::string itemsXmlPath()
{
    return std::string("items.xml");
}
} // namespace

TEST_CASE("ClueReader: items.xml is available to ResourceFS")
{
    REQUIRE(hh::ResourceFS::exists(itemsXmlPath()));
}

TEST_CASE("ClueReader::readFile + selectItems populates all four clue tiers")
{
    REQUIRE(hh::ResourceFS::exists(itemsXmlPath()));
    // selectItems uses the global RNG; seed it so the test is deterministic.
    PlantSeeds(12345);
    ClueReader reader;
    reader.readFile(itemsXmlPath());
    reader.selectItems();

    CHECK(!reader.getCluesJackpot().empty());
    CHECK(!reader.getCluesSpec().empty());
    CHECK(!reader.getCluesVague().empty());
    CHECK(!reader.getCluesWorthless().empty());
}

TEST_CASE("ClueReader: high/low item types are non-empty after selectItems")
{
    REQUIRE(hh::ResourceFS::exists(itemsXmlPath()));
    PlantSeeds(99);
    ClueReader reader;
    reader.readFile(itemsXmlPath());
    reader.selectItems();
    CHECK(!reader.getItemHigh().name.empty());
    CHECK(!reader.getItemHigh().type.empty());
    CHECK(!reader.getItemLow().name.empty());
    CHECK(!reader.getItemLow().type.empty());
}

TEST_CASE("ClueReader: same seed -> same selection (deterministic)")
{
    REQUIRE(hh::ResourceFS::exists(itemsXmlPath()));
    PlantSeeds(2026);
    ClueReader a;
    a.readFile(itemsXmlPath());
    a.selectItems();
    std::string aHigh = a.getItemHigh().name;
    std::string aLow  = a.getItemLow().name;

    PlantSeeds(2026);
    ClueReader b;
    b.readFile(itemsXmlPath());
    b.selectItems();
    CHECK_EQ(b.getItemHigh().name, aHigh);
    CHECK_EQ(b.getItemLow().name,  aLow);
}

TEST_MAIN()
