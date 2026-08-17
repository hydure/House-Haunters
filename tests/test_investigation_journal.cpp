#include "test_harness.hpp"

#include "game/InvestigationJournal.hpp"

TEST_CASE("investigation journal records unique shared evidence")
{
    auto& journal = InvestigationJournal::instance();
    journal.reset();

    CHECK(journal.discover("There are scorch marks.",
                           InvestigationJournal::Tier::VAGUE,
                           2));
    CHECK(!journal.discover("There are scorch marks.",
                            InvestigationJournal::Tier::VAGUE,
                            3));
    CHECK(journal.discover("Ghost died in a fire.",
                           InvestigationJournal::Tier::SPECIFIC,
                           1));

    REQUIRE(journal.entries().size() == 2);
    CHECK_EQ(journal.entries()[0].discoveredBy, 2);
    CHECK_EQ(journal.entries()[1].text, std::string("Ghost died in a fire."));
}

TEST_CASE("investigation journal reset starts a fresh run")
{
    auto& journal = InvestigationJournal::instance();
    journal.reset();
    journal.discover("Found a blade!", InvestigationJournal::Tier::JACKPOT, 4);
    REQUIRE(journal.entries().size() == 1);

    journal.reset();
    CHECK(journal.entries().empty());
    CHECK_EQ(std::string(InvestigationJournal::tierName(
                 InvestigationJournal::Tier::SPECIFIC)),
             std::string("SPECIFIC"));
}

TEST_MAIN()