#include "test_harness.hpp"

#include "game/WeaponSystem.hpp"

TEST_CASE("weapon damage rewards the deduced weakness")
{
    auto& weapons = WeaponSystem::instance();
    weapons.configure("fire", "water");

    CHECK_EQ(weapons.damageFor(WeaponSystem::Type::FIRE), 5);
    CHECK_EQ(weapons.damageFor(WeaponSystem::Type::WATER), 3);
    CHECK_EQ(weapons.damageFor(WeaponSystem::Type::BLADES), 1);
    CHECK_EQ(weapons.damageFor(WeaponSystem::Type::NONE), 1);
}

TEST_CASE("weapon choices cycle through every evidence category")
{
    CHECK(WeaponSystem::next(WeaponSystem::Type::NONE) == WeaponSystem::Type::FIRE);
    CHECK(WeaponSystem::next(WeaponSystem::Type::FIRE) == WeaponSystem::Type::BLADES);
    CHECK(WeaponSystem::next(WeaponSystem::Type::BLADES) == WeaponSystem::Type::WATER);
    CHECK(WeaponSystem::next(WeaponSystem::Type::WATER) == WeaponSystem::Type::FIRE);
    CHECK(WeaponSystem::fromString("BLADES") == WeaponSystem::Type::BLADES);
}

TEST_MAIN()