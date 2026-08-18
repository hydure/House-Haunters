// Config defaults + difficulty label table.

#include "test_harness.hpp"
#include "game/Config.hpp"

#include <string>

TEST_CASE("Config defaults match documented values")
{
    Config c;
    CHECK_EQ(c.width, 720);
    CHECK_EQ(c.height, 480);
    CHECK_EQ(c.fps, 60);
    CHECK_EQ(c.num_players, 1);
    CHECK_EQ(static_cast<int>(c.difficulty), static_cast<int>(Config::NORMAL));
    CHECK_EQ(c.time_Per_Phase, 90.0f);
    // Maps start empty -- character/title screens populate them.
    CHECK(c.player_map.empty());
    CHECK(c.char_map.empty());
}

TEST_CASE("Config::difficultyName covers every enum value")
{
    CHECK_EQ(std::string(Config::difficultyName(Config::EASY)),   std::string("EASY"));
    CHECK_EQ(std::string(Config::difficultyName(Config::NORMAL)), std::string("NORMAL"));
    CHECK_EQ(std::string(Config::difficultyName(Config::HARD)),   std::string("HARD"));
}

TEST_CASE("Config difficulty descriptions explain every gameplay lever")
{
    CHECK_EQ(std::string(Config::difficultyDescription(Config::EASY)),
             std::string("60% house | 120s prep | 8s ghost pings"));
    CHECK_EQ(std::string(Config::difficultyDescription(Config::NORMAL)),
             std::string("Standard house | 90s prep | 4s ghost pings"));
    CHECK_EQ(std::string(Config::difficultyDescription(Config::HARD)),
             std::string("150% house | 60s prep | 1.5s ghost pings"));
}

TEST_CASE("Config CHARACTER enum values are stable (gameplay maps depend on them)")
{
    // GameplayScreen and the networked-default mapping in HouseHaunters.cpp
    // assume these exact integer assignments. If you re-order this enum,
    // every player_map / char_map call site has to be re-audited.
    CHECK_EQ(static_cast<int>(Config::BRO), 0);
    CHECK_EQ(static_cast<int>(Config::SIS), 1);
    CHECK_EQ(static_cast<int>(Config::DAD), 2);
    CHECK_EQ(static_cast<int>(Config::MOM), 3);
}

TEST_MAIN()
