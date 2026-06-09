// EntityGroup owns Characters (shared_ptr) and Clues (unique_ptr) for the
// active game. Lookups (getCharacter / getClue) are id-keyed scans over
// those typed vectors -- ownership rules documented in EntityGroup.hpp.

#include "test_harness.hpp"
#include "components/EntityGroup.hpp"
#include "game/characters/Character.hpp"
#include "game/objects/Clue.hpp"

TEST_CASE("EntityGroup: starts empty")
{
    EntityGroup g;
    CHECK(g.getCharacters().empty());
    CHECK(g.getClues().empty());
    CHECK(g.getCharacter(1) == nullptr);
    CHECK(g.getClue(7) == nullptr);
}

TEST_CASE("EntityGroup: addCharacter / getCharacter by player_number")
{
    EntityGroup g;
    auto p1 = std::make_shared<Character>();
    p1->setPlayerNumber(1);
    auto p2 = std::make_shared<Character>();
    p2->setPlayerNumber(2);
    g.addCharacter(p1);
    g.addCharacter(p2);

    CHECK_EQ(g.getCharacters().size(), static_cast<size_t>(2));
    CHECK(g.getCharacter(1).get() == p1.get());
    CHECK(g.getCharacter(2).get() == p2.get());
    CHECK(g.getCharacter(99) == nullptr);
}

TEST_CASE("EntityGroup: addClue / getClue by clue_number")
{
    EntityGroup g;
    auto c1 = std::unique_ptr<Clue>(new Clue());
    c1->setClueNumber(10);
    Clue* c1_observer = c1.get();
    auto c2 = std::unique_ptr<Clue>(new Clue());
    c2->setClueNumber(20);
    Clue* c2_observer = c2.get();

    g.addClue(std::move(c1));
    g.addClue(std::move(c2));

    CHECK_EQ(g.getClues().size(), static_cast<size_t>(2));
    CHECK(g.getClue(10) == c1_observer);
    CHECK(g.getClue(20) == c2_observer);
    CHECK(g.getClue(0) == nullptr);
}

TEST_CASE("EntityGroup: characters are NOT attached as GameObject children (#14 rule)")
{
    // Top-level entities live in the typed vector, not the scene-graph
    // children list. This invariant is what lets EntityGroup::drawInArea
    // do its own ordered iteration. If somebody adds an `addChild(...)`
    // call inside addCharacter, this regression test catches it.
    EntityGroup g;
    auto p1 = std::make_shared<Character>();
    p1->setPlayerNumber(1);
    g.addCharacter(p1);
    // The contract is: addCharacter stores into `characters`, not `children`.
    // Confirm via the public accessor + the inherited (public) children list.
    CHECK_EQ(g.getCharacters().size(), static_cast<size_t>(1));
    CHECK(g.children.empty());
}

TEST_MAIN()
