#include "test_harness.hpp"

#include "game/RunSummary.hpp"
#include "game/screens/EndGameScreen.hpp"

TEST_CASE("run summary accumulates and finalizes round metrics")
{
    auto& summary = RunSummary::instance();
    summary.begin(Config::HARD, 3, 90);
    summary.update(65.8f);
    summary.recordDeath();
    summary.recordDamage(5);
    summary.recordDamage(3);
    summary.finish(true, 7);

    CHECK(summary.complete());
    CHECK(summary.won());
    CHECK_EQ(summary.players(), 3);
    CHECK_EQ(summary.rooms(), 90);
    CHECK_EQ(summary.deaths(), 1);
    CHECK_EQ(summary.damageDealt(), 8);
    CHECK_EQ(summary.evidenceFound(), 7);
    CHECK_EQ(RunSummary::formatDuration(summary.elapsedSeconds()), std::string("1:05"));
}

TEST_CASE("completed run summary ignores late simulation events")
{
    auto& summary = RunSummary::instance();
    summary.begin(Config::EASY, 1, 12);
    summary.finish(false, 2);
    summary.update(10.f);
    summary.recordDeath();
    summary.recordDamage(99);

    CHECK(!summary.won());
    CHECK_EQ(summary.elapsedSeconds(), 0.f);
    CHECK_EQ(summary.deaths(), 0);
    CHECK_EQ(summary.damageDealt(), 0);
}

TEST_CASE("end screen offers direct rematch and explicit title return")
{
    CHECK_EQ(std::string(EndGameScreen::destinationForButton("A")),
             std::string("GamePlay"));
    CHECK_EQ(std::string(EndGameScreen::destinationForButton("START")),
             std::string("GamePlay"));
    CHECK_EQ(std::string(EndGameScreen::destinationForButton("B")),
             std::string("Title"));
    CHECK_EQ(std::string(EndGameScreen::destinationForButton("X")),
             std::string(""));
}

TEST_MAIN()