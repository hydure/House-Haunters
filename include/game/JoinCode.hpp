#ifndef GAME_JOIN_CODE_HPP
#define GAME_JOIN_CODE_HPP

#include <array>
#include <string>

//////////////////////////////
// JoinCode.hpp
//
// Shared encode/decode for the 4-digit ROOM CODE shown on HostScreen and
// entered on JoinScreen. The code is no longer an encoded IP address --
// instead the host generates a random 4-digit identifier (1000..9999)
// and advertises it on the LAN over UDP. Joiners broadcast a discovery
// probe carrying the same code; NetworkManager resolves the host's
// actual IP from the reply packet and then opens the usual TCP
// connection on port 53353.
//
// Examples:
//   1234            -> "1234"
//   42              -> "0042"   (zero-padded for display + transport)
//
// Why exactly 4 digits?
//   * Memorable & easy to read out loud over voice chat.
//   * 9000 valid codes (1000..9999) keeps collisions very unlikely while
//     avoiding the awkward leading-zero "0000" case that looks like an
//     uninitialized field.
//   * Fits four big slots on screen so the touch/controller picker is
//     uncluttered.
//
// All functions are inline and side-effect-free so they're cheap to call
// every frame and trivially unit-testable without sockets or globals.
//////////////////////////////
namespace JoinCode {

// Code is always exactly 4 ASCII digits ('0'..'9').
constexpr int kCodeLen   = 4;
constexpr int kCodeMin   = 1000; // smallest code we generate (4 digits, no
                                 // leading-zero ambiguity)
constexpr int kCodeMax   = 9999; // largest representable code

inline char digitChar(int d)
{
    if (d < 0) d = 0;
    if (d > 9) d = 9;
    return static_cast<char>('0' + d);
}

// Returns true and writes 0..9 into *out when ch is a decimal digit.
// Returns false for anything else (letters, punctuation, whitespace).
inline bool digitValue(char ch, int* out)
{
    if (ch >= '0' && ch <= '9') { *out = ch - '0'; return true; }
    return false;
}

// True if ch is one of '0'..'9'. Convenience for input filtering.
inline bool isDecimalDigit(char ch)
{
    return ch >= '0' && ch <= '9';
}

// 4-char zero-padded decimal of the room code.
// Negative numbers clamp to 0 and values > 9999 clamp to 9999 so live
// UI previews never overflow the buffer; callers that need strict
// validation should use decode().
inline std::string encode(int code)
{
    if (code < 0)    code = 0;
    if (code > 9999) code = 9999;
    std::string s(kCodeLen, '0');
    s[0] = digitChar((code / 1000) % 10);
    s[1] = digitChar((code /  100) % 10);
    s[2] = digitChar((code /   10) % 10);
    s[3] = digitChar( code         % 10);
    return s;
}

// Pack 4 decimal digits back into an integer 0..9999. Out-of-range
// digit slots clamp to [0,9]. Used by the live preview while the
// user is mid-edit; strict validation lives in decode().
inline int packDigits(const std::array<int, kCodeLen>& d)
{
    int v = 0;
    for (int i = 0; i < kCodeLen; ++i) {
        int x = d[i];
        if (x < 0) x = 0;
        if (x > 9) x = 9;
        v = v * 10 + x;
    }
    return v;
}

// Strict parse: exactly 4 ASCII decimal digits. Returns false (and
// leaves *out untouched) on any other input. The returned code is
// allowed to be 0000..9999; callers that need "non-trivial" codes can
// additionally enforce >= kCodeMin.
inline bool decode(const std::string& code, int* out)
{
    if (static_cast<int>(code.size()) != kCodeLen) return false;
    int v = 0;
    for (int i = 0; i < kCodeLen; ++i) {
        int d = 0;
        if (!digitValue(code[i], &d)) return false;
        v = v * 10 + d;
    }
    *out = v;
    return true;
}

} // namespace JoinCode

#endif
