// NetworkManager (item #15).
//
// We can't bind real TCP sockets in a unit test reliably, so this suite
// covers the parts that don't require I/O:
//   * HH_NET parsing (configureFromEnv) for OFFLINE / HOST / CLIENT
//   * Bad input -> graceful OFFLINE downgrade
//   * interceptLocalGamepad() respects the current mode
// Lockstep tick exchange is covered by manual integration with two
// processes (see NetworkManager.hpp comments).

#include "test_harness.hpp"
#include "engine/NetworkManager.hpp"

#include <cstdlib>
#include <string>

namespace {
// Portable env var setter; the standard library splits these between
// POSIX (setenv/unsetenv) and Windows (_putenv_s).
void setEnv(const char* key, const char* value)
{
#if defined(_WIN32)
    if (value == nullptr) {
        _putenv_s(key, "");
    } else {
        _putenv_s(key, value);
    }
#else
    if (value == nullptr) {
        unsetenv(key);
    } else {
        setenv(key, value, 1);
    }
#endif
}

void resetManager()
{
    auto& nm = NetworkManager::instance();
    nm.setMode(NetworkManager::Mode::OFFLINE);
    nm.disconnect();
    setEnv("HH_NET", "");
}
} // namespace

TEST_CASE("HH_NET unset -> OFFLINE")
{
    resetManager();
    NetworkManager::instance().configureFromEnv();
    CHECK(NetworkManager::instance().isOffline());
}

TEST_CASE("HH_NET empty string -> OFFLINE")
{
    resetManager();
    setEnv("HH_NET", "");
    NetworkManager::instance().configureFromEnv();
    CHECK(NetworkManager::instance().isOffline());
}

TEST_CASE("HH_NET host:3 -> HOST mode + seeded")
{
    resetManager();
    setEnv("HH_NET", "host:3");
    auto& nm = NetworkManager::instance();
    nm.configureFromEnv();
    CHECK(nm.mode() == NetworkManager::Mode::HOST);
    CHECK(nm.isNetworked());
    // Seed must be deterministic + nonzero so PlantSeeds has a stable
    // value to broadcast to peers.
    CHECK(nm.seed() > 0);
    resetManager();
}

TEST_CASE("HH_NET host:N with custom port parses cleanly")
{
    resetManager();
    setEnv("HH_NET", "host:2:54321");
    auto& nm = NetworkManager::instance();
    nm.configureFromEnv();
    CHECK(nm.mode() == NetworkManager::Mode::HOST);
    // Port is stored privately, but we can at least confirm mode survived
    // the extra colon-separated token.
    CHECK(nm.isNetworked());
    resetManager();
}

TEST_CASE("HH_NET client:HOST -> CLIENT mode")
{
    resetManager();
    setEnv("HH_NET", "client:127.0.0.1");
    auto& nm = NetworkManager::instance();
    nm.configureFromEnv();
    CHECK(nm.mode() == NetworkManager::Mode::CLIENT);
    CHECK(nm.isNetworked());
    resetManager();
}

TEST_CASE("HH_NET client:<bad address> -> OFFLINE")
{
    resetManager();
    setEnv("HH_NET", "client:this-is-not-a-real-host.invalid");
    auto& nm = NetworkManager::instance();
    nm.configureFromEnv();
    // sf::IpAddress(...) returns IpAddress::None for unresolvable hostnames;
    // configureFromEnv must downgrade cleanly rather than leave us in a
    // half-configured CLIENT state.
    CHECK(nm.isOffline());
    resetManager();
}

TEST_CASE("HH_NET garbage -> OFFLINE")
{
    resetManager();
    setEnv("HH_NET", "nonsense");
    NetworkManager::instance().configureFromEnv();
    CHECK(NetworkManager::instance().isOffline());
    resetManager();
}

TEST_CASE("interceptLocalGamepad in OFFLINE returns false (local fallback)")
{
    resetManager();
    NetworkManager::instance().configureFromEnv();
    CHECK_EQ(NetworkManager::instance().interceptLocalGamepad("A", true), false);
    CHECK_EQ(NetworkManager::instance().interceptLocalGamepad("UP", false), false);
}

TEST_CASE("interceptLocalGamepad in HOST/CLIENT returns true (network path)")
{
    resetManager();
    auto& nm = NetworkManager::instance();
    // Flip mode directly: we're not actually opening sockets, but the
    // intercept contract is the same regardless of whether handshake ran.
    nm.setMode(NetworkManager::Mode::HOST);
    CHECK_EQ(nm.interceptLocalGamepad("A", true), true);
    nm.setMode(NetworkManager::Mode::CLIENT);
    CHECK_EQ(nm.interceptLocalGamepad("B", false), true);
    resetManager();
}

TEST_MAIN()
