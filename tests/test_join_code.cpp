// JoinCode (host/join multiplayer room codes).
//
// Covers the encode/decode pair shared by HostScreen and JoinScreen.
// All paths are pure (no sockets, no globals) so the round-trip checks
// run instantly without touching the network stack.
//
// New format (vs the previous 12-digit IP encoding): the JOIN CODE is
// a 4-digit zero-padded decimal room number (0000..9999). The host
// generates one in beginHostListening() and advertises it on UDP;
// joiners type the same 4 digits and let NetworkManager's UDP
// discovery resolve the actual IP.

#include "test_harness.hpp"
#include "game/JoinCode.hpp"

#include <array>
#include <string>

namespace {

// Helper: build the digit array from a 4-char decimal string. Assumes
// the caller knows the string is exactly 4 decimal chars.
std::array<int, JoinCode::kCodeLen> digitsFromDec(const std::string& s)
{
    std::array<int, JoinCode::kCodeLen> d{};
    for (int i = 0; i < JoinCode::kCodeLen; ++i) {
        int v = 0;
        JoinCode::digitValue(s[i], &v);
        d[i] = v;
    }
    return d;
}

} // namespace

TEST_CASE("digitChar maps 0..9 to '0'..'9' and clamps the rest")
{
    CHECK_EQ(JoinCode::digitChar(0), '0');
    CHECK_EQ(JoinCode::digitChar(5), '5');
    CHECK_EQ(JoinCode::digitChar(9), '9');
    // Out-of-range inputs get clamped (defensive: gamepad cursor edits
    // could otherwise produce garbage values).
    CHECK_EQ(JoinCode::digitChar(-3), '0');
    CHECK_EQ(JoinCode::digitChar(10), '9');
    CHECK_EQ(JoinCode::digitChar(99), '9');
}

TEST_CASE("digitValue parses '0'..'9' only")
{
    int v = -1;
    CHECK(JoinCode::digitValue('0', &v)); CHECK_EQ(v, 0);
    CHECK(JoinCode::digitValue('5', &v)); CHECK_EQ(v, 5);
    CHECK(JoinCode::digitValue('9', &v)); CHECK_EQ(v, 9);
    // Anything else fails (hex letters are NOT accepted -- the whole
    // point of the format switch was to ditch A-F).
    CHECK(!JoinCode::digitValue('A', &v));
    CHECK(!JoinCode::digitValue('f', &v));
    CHECK(!JoinCode::digitValue(' ', &v));
    CHECK(!JoinCode::digitValue('!', &v));
    CHECK(!JoinCode::digitValue('.', &v));
}

TEST_CASE("isDecimalDigit recognizes the 0-9 range")
{
    CHECK(JoinCode::isDecimalDigit('0'));
    CHECK(JoinCode::isDecimalDigit('9'));
    CHECK(!JoinCode::isDecimalDigit('A'));
    CHECK(!JoinCode::isDecimalDigit('a'));
    CHECK(!JoinCode::isDecimalDigit('/'));     // '/' is right before '0'
    CHECK(!JoinCode::isDecimalDigit(':'));     // ':' is right after '9'
}

TEST_CASE("encode formats a code as 4 zero-padded decimal digits")
{
    CHECK_EQ(JoinCode::encode(1234), std::string("1234"));
    CHECK_EQ(JoinCode::encode(42),   std::string("0042"));
    CHECK_EQ(JoinCode::encode(0),    std::string("0000"));
    CHECK_EQ(JoinCode::encode(9999), std::string("9999"));
}

TEST_CASE("encode clamps out-of-range inputs")
{
    // Negative -> 0000. > 9999 -> 9999. These guards keep the live UI
    // preview printable even when packDigits is mid-edit.
    CHECK_EQ(JoinCode::encode(-1),     std::string("0000"));
    CHECK_EQ(JoinCode::encode(10000),  std::string("9999"));
    CHECK_EQ(JoinCode::encode(123456), std::string("9999"));
}

TEST_CASE("decode is the inverse of encode for valid 4-digit strings")
{
    const int kCodes[] = { 0, 1, 42, 1000, 1234, 5000, 9999 };
    for (int in : kCodes) {
        const std::string s = JoinCode::encode(in);
        int out = -1;
        CHECK(JoinCode::decode(s, &out));
        CHECK_EQ(out, in);
    }
}

TEST_CASE("decode rejects wrong length")
{
    int out = 42;
    CHECK(!JoinCode::decode("",       &out));   // empty
    CHECK(!JoinCode::decode("123",    &out));   // 3 chars
    CHECK(!JoinCode::decode("12345",  &out));   // 5 chars
    CHECK(!JoinCode::decode("0001234",&out));   // 7 chars
    // Output left untouched on failure.
    CHECK_EQ(out, 42);
}

TEST_CASE("decode rejects non-decimal characters")
{
    int out = 0;
    CHECK(!JoinCode::decode("ABCD", &out));     // pure letters
    CHECK(!JoinCode::decode("123A", &out));     // single bad char
    CHECK(!JoinCode::decode("1.23", &out));     // dot
    CHECK(!JoinCode::decode("12 4", &out));     // whitespace
    CHECK(!JoinCode::decode("-123", &out));     // sign
}

TEST_CASE("packDigits assembles 4 digits into a single integer")
{
    std::array<int, JoinCode::kCodeLen> d = { 1, 2, 3, 4 };
    CHECK_EQ(JoinCode::packDigits(d), 1234);

    d = { 0, 0, 4, 2 };
    CHECK_EQ(JoinCode::packDigits(d), 42);

    d = { 9, 9, 9, 9 };
    CHECK_EQ(JoinCode::packDigits(d), 9999);
}

TEST_CASE("packDigits clamps slot values to [0,9]")
{
    // Out-of-range digit slots (negative or > 9) shouldn't blow up the
    // preview text; each clamps independently before assembly.
    std::array<int, JoinCode::kCodeLen> d = { -3, 5, 12, 999 };
    CHECK_EQ(JoinCode::packDigits(d), 0 * 1000 + 5 * 100 + 9 * 10 + 9);
}

TEST_CASE("packDigits round-trips with encode/decode for a strict-valid code")
{
    const auto d = digitsFromDec("4321");
    int decoded = -1;
    CHECK(JoinCode::decode("4321", &decoded));
    CHECK_EQ(JoinCode::packDigits(d), decoded);
    CHECK_EQ(JoinCode::packDigits(d), 4321);
}

TEST_CASE("kCodeMin / kCodeMax describe the host-generated range")
{
    // beginHostListening generates codes in [kCodeMin, kCodeMax] so the
    // displayed string is always 4 visible digits with no awkward
    // leading zeros. Pin those constants so a future tweak forces a
    // matching test update.
    CHECK_EQ(JoinCode::kCodeMin, 1000);
    CHECK_EQ(JoinCode::kCodeMax, 9999);
    CHECK_EQ(JoinCode::kCodeLen, 4);
}

TEST_MAIN()
