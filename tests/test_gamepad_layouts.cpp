// Controller layout regression tests.
//
// What this exercises:
//   * Gamepad::layoutFor(vid, pid) routes every supported vendor/product
//     id pair to the right LAYOUT enum value (PlayStation, Xbox, Steam,
//     Switch, 8BitDo, third-party Xbox-likes, generic fallback).
//   * Gamepad::setLayout() populates the button table for every layout
//     so every documented button name has a binding -- the historic
//     bug was that PS3 / GENERIC produced an empty table and therefore
//     never emitted gamepad events at all.
//   * KEYBOARD layout binds the expected keyboard keys (Z/X/C/V plus
//     arrows / Escape / Return / Tab / Q / E).
//
// These tests never instantiate sf::Joystick, so they run identically
// on a developer laptop with no controllers plugged in.

#include "test_harness.hpp"
#include "engine/Gamepad.hpp"

namespace {

// All non-keyboard layouts must bind these button names, otherwise the
// in-game character-screen / pause-menu / gameplay-screen handlers
// can't receive input from that controller family.
const std::vector<std::string> kRequiredButtonNames = {
    "A", "B", "X", "Y",
    "UP", "DOWN", "LEFT", "RIGHT",
    "START", "MENU", "SELECT",
    "L", "R"
};

const std::vector<Gamepad::LAYOUT> kPhysicalLayouts = {
    Gamepad::GENERIC,
    Gamepad::PS3, Gamepad::PS4, Gamepad::PS5,
    Gamepad::XB360, Gamepad::XB1, Gamepad::XBSERIES, Gamepad::XBOX_GENERIC,
    Gamepad::STEAM, Gamepad::SWITCH_PRO, Gamepad::EIGHT_BITDO
};

} // namespace

TEST_CASE("layoutFor: Sony VIDs map to the right PlayStation family")
{
    CHECK_EQ(Gamepad::layoutFor(0x054cu, 0x0268u), Gamepad::PS3);
    CHECK_EQ(Gamepad::layoutFor(0x054cu, 0x05c4u), Gamepad::PS4);
    CHECK_EQ(Gamepad::layoutFor(0x054cu, 0x09ccu), Gamepad::PS4);
    CHECK_EQ(Gamepad::layoutFor(0x054cu, 0x0ba0u), Gamepad::PS4);
    CHECK_EQ(Gamepad::layoutFor(0x054cu, 0x0ce6u), Gamepad::PS5);
    CHECK_EQ(Gamepad::layoutFor(0x054cu, 0x0df2u), Gamepad::PS5);
    // Unknown Sony PIDs still resolve to a real PlayStation layout so
    // newly-released DualSense revisions aren't dead on arrival.
    const auto unknownSony = Gamepad::layoutFor(0x054cu, 0xffffu);
    CHECK(unknownSony == Gamepad::PS3 || unknownSony == Gamepad::PS4
          || unknownSony == Gamepad::PS5);
}

TEST_CASE("layoutFor: Microsoft VIDs map to the right Xbox family")
{
    CHECK_EQ(Gamepad::layoutFor(0x045eu, 0x028eu), Gamepad::XB360);
    CHECK_EQ(Gamepad::layoutFor(0x045eu, 0x028fu), Gamepad::XB360);
    CHECK_EQ(Gamepad::layoutFor(0x045eu, 0x02d1u), Gamepad::XB1);
    CHECK_EQ(Gamepad::layoutFor(0x045eu, 0x02ddu), Gamepad::XB1);
    CHECK_EQ(Gamepad::layoutFor(0x045eu, 0x02e0u), Gamepad::XB1);
    CHECK_EQ(Gamepad::layoutFor(0x045eu, 0x02e3u), Gamepad::XB1);
    CHECK_EQ(Gamepad::layoutFor(0x045eu, 0x02eau), Gamepad::XB1);
    CHECK_EQ(Gamepad::layoutFor(0x045eu, 0x02fdu), Gamepad::XB1);
    CHECK_EQ(Gamepad::layoutFor(0x045eu, 0x0b00u), Gamepad::XBSERIES);
    CHECK_EQ(Gamepad::layoutFor(0x045eu, 0x0b05u), Gamepad::XBSERIES);
    CHECK_EQ(Gamepad::layoutFor(0x045eu, 0x0b12u), Gamepad::XBSERIES);
    CHECK_EQ(Gamepad::layoutFor(0x045eu, 0x0b13u), Gamepad::XBSERIES);
    CHECK_EQ(Gamepad::layoutFor(0x045eu, 0x0b20u), Gamepad::XBSERIES);
    // Unknown Microsoft PIDs fall back to XBOX_GENERIC, not GENERIC,
    // because the XInput button map is more reliable than the SDL one
    // for first-party hardware revisions we haven't catalogued yet.
    CHECK_EQ(Gamepad::layoutFor(0x045eu, 0xffffu), Gamepad::XBOX_GENERIC);
}

TEST_CASE("layoutFor: Steam / Switch / 8BitDo / third-party Xbox-likes")
{
    CHECK_EQ(Gamepad::layoutFor(0x28deu, 0x1142u), Gamepad::STEAM);
    CHECK_EQ(Gamepad::layoutFor(0x28deu, 0x1102u), Gamepad::STEAM);
    CHECK_EQ(Gamepad::layoutFor(0x28deu, 0x1201u), Gamepad::STEAM);

    CHECK_EQ(Gamepad::layoutFor(0x057eu, 0x2009u), Gamepad::SWITCH_PRO);
    CHECK_EQ(Gamepad::layoutFor(0x057eu, 0x2017u), Gamepad::SWITCH_PRO);
    CHECK_EQ(Gamepad::layoutFor(0x057eu, 0x2006u), Gamepad::SWITCH_PRO);
    CHECK_EQ(Gamepad::layoutFor(0x057eu, 0x2007u), Gamepad::SWITCH_PRO);

    CHECK_EQ(Gamepad::layoutFor(0x2dc8u, 0x0001u), Gamepad::EIGHT_BITDO);

    // DragonRise, PDP, PowerA, Razer all ship Xbox-style mappings.
    CHECK_EQ(Gamepad::layoutFor(0x0079u, 0x0011u), Gamepad::XBOX_GENERIC);
    CHECK_EQ(Gamepad::layoutFor(0x0e6fu, 0x0247u), Gamepad::XBOX_GENERIC);
    CHECK_EQ(Gamepad::layoutFor(0x24c6u, 0x5d04u), Gamepad::XBOX_GENERIC);
    CHECK_EQ(Gamepad::layoutFor(0x1532u, 0x0a14u), Gamepad::XBOX_GENERIC);
}

TEST_CASE("layoutFor: unknown vendor falls back to GENERIC")
{
    CHECK_EQ(Gamepad::layoutFor(0xdeadu, 0xbeefu), Gamepad::GENERIC);
    CHECK_EQ(Gamepad::layoutFor(0x0000u, 0x0000u), Gamepad::GENERIC);
}

TEST_CASE("Every physical layout binds A/B/X/Y/UP/DOWN/LEFT/RIGHT/START/MENU/SELECT/L/R")
{
    // The original bug: PS3 and GENERIC went through setLayout() but
    // never populated button_map, so those controllers couldn't emit
    // any events. This test guards every documented binding name on
    // every supported layout.
    for (const auto& layout : kPhysicalLayouts) {
        Gamepad g(0);
        g.setLayout(layout);
        for (const auto& name : kRequiredButtonNames) {
            const bool ok = g.hasBinding(name);
            // CHECK can't capture extra context, so emit a diagnostic
            // line before failing -- otherwise a future regression
            // here is needle-in-haystack to debug.
            if (!ok) {
                std::cerr << "    layout " << static_cast<int>(layout)
                          << " is missing binding for '" << name << "'"
                          << std::endl;
            }
            CHECK(ok);
        }
    }
}

TEST_CASE("Keyboard layout binds A/B/X/Y and arrows and START/MENU/SELECT/L/R")
{
    Gamepad g(0);
    g.setLayout(Gamepad::KEYBOARD);
    for (const auto& name : kRequiredButtonNames) {
        const bool ok = g.hasBinding(name);
        if (!ok) {
            std::cerr << "    KEYBOARD layout is missing binding for '"
                      << name << "'" << std::endl;
        }
        CHECK(ok);
    }
}

TEST_CASE("Constructed Gamepad reports its assigned controller index")
{
    Gamepad g(7);
    CHECK_EQ(g.getIndex(), 7);
    g.setIndex(3);
    CHECK_EQ(g.getIndex(), 3);
}

TEST_CASE("setConnected toggles the connected flag")
{
    Gamepad g(0);
    CHECK(g.isConnected());
    g.setConnected(false);
    CHECK(!g.isConnected());
    g.setConnected(true);
    CHECK(g.isConnected());
}

TEST_MAIN()
