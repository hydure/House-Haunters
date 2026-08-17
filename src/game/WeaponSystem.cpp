#include "game/WeaponSystem.hpp"

#include <algorithm>
#include <cctype>

WeaponSystem& WeaponSystem::instance()
{
    static WeaponSystem system;
    return system;
}

void WeaponSystem::configure(const std::string& highDamageType,
                             const std::string& lowDamageType)
{
    highDamageType_ = fromString(highDamageType);
    lowDamageType_ = fromString(lowDamageType);
}

int WeaponSystem::damageFor(Type type) const
{
    if (type == Type::NONE) return 1;
    if (type == highDamageType_) return 5;
    if (type == lowDamageType_) return 3;
    return 1;
}

WeaponSystem::Type WeaponSystem::next(Type type)
{
    switch (type) {
        case Type::NONE:   return Type::FIRE;
        case Type::FIRE:   return Type::BLADES;
        case Type::BLADES: return Type::WATER;
        case Type::WATER:  return Type::FIRE;
    }
    return Type::FIRE;
}

WeaponSystem::Type WeaponSystem::fromString(const std::string& type)
{
    std::string normalized = type;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
        [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
    if (normalized == "fire") return Type::FIRE;
    if (normalized == "blades") return Type::BLADES;
    if (normalized == "water") return Type::WATER;
    return Type::NONE;
}

const char* WeaponSystem::name(Type type)
{
    switch (type) {
        case Type::NONE:   return "NONE";
        case Type::FIRE:   return "FIRE";
        case Type::BLADES: return "BLADES";
        case Type::WATER:  return "WATER";
    }
    return "NONE";
}