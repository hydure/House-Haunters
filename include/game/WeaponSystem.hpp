#ifndef WEAPON_SYSTEM_HPP
#define WEAPON_SYSTEM_HPP

#include <string>

class WeaponSystem
{
public:
    enum class Type {
        NONE,
        FIRE,
        BLADES,
        WATER
    };

    static WeaponSystem& instance();

    void configure(const std::string& highDamageType,
                   const std::string& lowDamageType);
    int damageFor(Type type) const;

    static Type next(Type type);
    static Type fromString(const std::string& type);
    static const char* name(Type type);

private:
    Type highDamageType_ = Type::NONE;
    Type lowDamageType_ = Type::NONE;
};

#endif