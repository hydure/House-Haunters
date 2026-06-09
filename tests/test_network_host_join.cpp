// NetworkManager: real-loopback exercise of the new host/join flow
// (beginHostListening + pollAccept on one side, clientConnectWithTimeout
// on the other). This validates the wire path end-to-end without
// requiring two processes:
//   1. main thread plays the host -> beginHostListening, then polls
//      pollAccept() in a short busy loop while
//   2. a background thread plays the client -> clientConnectWithTimeout
//
// Because NetworkManager is a singleton we can't host AND join in the
// same process safely, so the client thread builds its own
// sf::TcpSocket-based "fake client" that speaks the documented handshake
// protocol. The host path under test is the real production code.
//
// A clientConnectWithTimeout negative test (no listener -> bounded
// timeout, returns false, mode left as CLIENT) covers the new timeout
// API directly.

#include "test_harness.hpp"
#include "engine/NetworkManager.hpp"

#include <SFML/Network.hpp>
#include <SFML/System.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace {

// Pick a port unlikely to clash with anything else on the dev box.
constexpr unsigned short kTestPort = 53999;

// Protocol constants must match the values inside NetworkManager.cpp.
constexpr sf::Uint32 MAGIC_HANDSHAKE  = 0x48484E45u; // 'HHNE'
constexpr sf::Uint32 PROTOCOL_VERSION = 1u;

void resetManager()
{
    auto& nm = NetworkManager::instance();
    nm.setMode(NetworkManager::Mode::OFFLINE);
    nm.disconnect();
}

// A bare-bones client that connects to the host, reads the handshake
// packet, validates the magic, and reports the seed/slot via atomics.
struct FakeClient {
    std::atomic<bool>     done{false};
    std::atomic<bool>     ok{false};
    std::atomic<uint32_t> seed{0};
    std::atomic<uint32_t> total{0};
    std::atomic<uint32_t> slot{99};
    std::thread           th;

    void run(sf::IpAddress host, unsigned short port)
    {
        th = std::thread([this, host, port]() {
            sf::TcpSocket sock;
            // 3-second connect timeout to keep the test fast on failure.
            if (sock.connect(host, port, sf::seconds(3.f)) != sf::Socket::Done) {
                done = true;
                return;
            }
            sock.setBlocking(true);
            sf::Packet pkt;
            if (sock.receive(pkt) != sf::Socket::Done) {
                done = true;
                return;
            }
            sf::Uint32 magic = 0, ver = 0, s = 0, n = 0, slt = 0;
            if (!(pkt >> magic >> ver >> s >> n >> slt)) {
                done = true;
                return;
            }
            if (magic != MAGIC_HANDSHAKE || ver != PROTOCOL_VERSION) {
                done = true;
                return;
            }
            seed  = s;
            total = n;
            slot  = slt;
            ok    = true;
            // Hold the socket open just long enough for the host to flag
            // the handshake complete. Closing immediately is fine -- the
            // host already finished the send.
            done = true;
        });
    }

    void join()
    {
        if (th.joinable()) th.join();
    }
};

} // namespace

TEST_CASE("beginHostListening rejects calls outside HOST mode")
{
    resetManager();
    auto& nm = NetworkManager::instance();
    nm.setMode(NetworkManager::Mode::OFFLINE);
    CHECK(!nm.beginHostListening());
    nm.setMode(NetworkManager::Mode::CLIENT);
    CHECK(!nm.beginHostListening());
    resetManager();
}

TEST_CASE("pollAccept returns false until a client connects")
{
    resetManager();
    auto& nm = NetworkManager::instance();
    nm.setMode(NetworkManager::Mode::HOST);
    nm.setExpectedPeers(1);
    nm.setSeed(0);
    // Bump the port on every retry attempt below -- if the previous
    // test failed mid-flight the port could still be in TIME_WAIT.
    unsigned short port = kTestPort + 1;
    nm.setHost(sf::IpAddress::Any, port);
    REQUIRE(nm.beginHostListening());

    bool err = false;
    // No client has connected yet; pollAccept must report not-done.
    CHECK(!nm.pollAccept(&err));
    CHECK(!err);
    CHECK_EQ(nm.acceptedPeers(), 0);
    // totalPlayers_ now starts at 1 (just the host) and grows with each
    // accepted peer, so before any client connects the slot space is 1.
    CHECK_EQ(nm.totalPlayers(), 1);
    CHECK_EQ(nm.expectedPeers(), 1);

    resetManager();
}

TEST_CASE("host+client handshake completes over loopback")
{
    resetManager();
    auto& nm = NetworkManager::instance();
    nm.setMode(NetworkManager::Mode::HOST);
    nm.setExpectedPeers(1);
    nm.setSeed(0);
    unsigned short port = kTestPort + 2;
    nm.setHost(sf::IpAddress::Any, port);
    REQUIRE(nm.beginHostListening());
    const long hostSeed = nm.seed();
    CHECK(hostSeed > 0);
    // Pre-accept: only the host occupies the slot space.
    CHECK_EQ(nm.totalPlayers(), 1);

    // Spin up the fake client.
    FakeClient client;
    client.run(sf::IpAddress::LocalHost, port);

    // Drive pollAccept on the main thread. Bound the wait so a failure
    // in the client side doesn't hang CTest.
    bool done = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        bool err = false;
        if (nm.pollAccept(&err)) { done = true; break; }
        CHECK(!err);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(done);
    CHECK_EQ(nm.acceptedPeers(), 1);

    client.join();
    CHECK(client.ok);
    CHECK_EQ(client.seed.load(),  static_cast<uint32_t>(hostSeed));
    CHECK_EQ(client.total.load(), static_cast<uint32_t>(2));
    CHECK_EQ(client.slot.load(),  static_cast<uint32_t>(1));

    resetManager();
}

TEST_CASE("clientConnectWithTimeout fails fast when nothing is listening")
{
    resetManager();
    auto& nm = NetworkManager::instance();
    nm.setMode(NetworkManager::Mode::CLIENT);
    // Use a port we definitely never opened so the host is unreachable.
    nm.setHost(sf::IpAddress::LocalHost, static_cast<unsigned short>(kTestPort + 50));

    auto start = std::chrono::steady_clock::now();
    const bool ok = nm.clientConnectWithTimeout(sf::seconds(1.f));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    CHECK(!ok);
    // Must respect the timeout -- give a generous upper bound to account
    // for scheduler jitter, but it must NOT hang for the OS default
    // (~minute-long) TCP backoff.
    CHECK(elapsed.count() < 4000);
    resetManager();
}

TEST_CASE("clientConnectWithTimeout rejects calls outside CLIENT mode")
{
    resetManager();
    auto& nm = NetworkManager::instance();
    nm.setMode(NetworkManager::Mode::OFFLINE);
    CHECK(!nm.clientConnectWithTimeout(sf::seconds(1.f)));
    nm.setMode(NetworkManager::Mode::HOST);
    CHECK(!nm.clientConnectWithTimeout(sf::seconds(1.f)));
    resetManager();
}

TEST_CASE("beginHostListening generates a 4-digit room code in [1000, 9999]")
{
    resetManager();
    auto& nm = NetworkManager::instance();
    nm.setMode(NetworkManager::Mode::HOST);
    nm.setExpectedPeers(1);
    nm.setRoomCode(0); // explicitly request auto-generation
    nm.setSeed(0);
    nm.setHost(sf::IpAddress::Any, static_cast<unsigned short>(kTestPort + 60));
    REQUIRE(nm.beginHostListening());

    const int code = nm.roomCode();
    CHECK(code >= 1000);
    CHECK(code <= 9999);
    resetManager();
}

TEST_CASE("beginHostListening honors an explicitly-set room code")
{
    resetManager();
    auto& nm = NetworkManager::instance();
    nm.setMode(NetworkManager::Mode::HOST);
    nm.setExpectedPeers(1);
    nm.setSeed(0);
    nm.setRoomCode(4242); // host UI / test pins a specific code
    nm.setHost(sf::IpAddress::Any, static_cast<unsigned short>(kTestPort + 61));
    REQUIRE(nm.beginHostListening());
    CHECK_EQ(nm.roomCode(), 4242);
    resetManager();
}

TEST_CASE("clientDiscoverHost resolves the host's address via UDP probe")
{
    // End-to-end discovery: host opens its UDP socket via
    // beginHostListening(), then we poll a few times to drain the
    // probe issued by clientDiscoverHost. The discovered IP should be
    // loopback because the probe went to 127.0.0.1.
    resetManager();
    auto& nm = NetworkManager::instance();
    nm.setMode(NetworkManager::Mode::HOST);
    nm.setExpectedPeers(1);
    nm.setSeed(0);
    nm.setRoomCode(7777);
    nm.setHost(sf::IpAddress::Any, static_cast<unsigned short>(kTestPort + 62));
    REQUIRE(nm.beginHostListening());

    // Run the joiner on a worker thread (it blocks while polling for
    // the UDP reply). The main thread pumps pollAccept(), which also
    // services the discovery socket and answers the probe.
    std::atomic<bool>        joinerDone{false};
    std::atomic<bool>        joinerOk{false};
    sf::IpAddress            discovered = sf::IpAddress::None;
    std::thread joiner([&]() {
        joinerOk = nm.clientDiscoverHost(7777, sf::seconds(3.f), &discovered);
        joinerDone = true;
    });

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (!joinerDone.load() && std::chrono::steady_clock::now() < deadline) {
        bool err = false;
        nm.pollAccept(&err); // drains the UDP probe and replies
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    joiner.join();

    CHECK(joinerOk.load());
    // The discovered address depends on which interface answered first --
    // on Windows the broadcast probe routes through the LAN NIC, so the
    // host's reply comes back from the LAN IP, not 127.0.0.1. All we
    // care about for the contract is that *some* host address was
    // resolved (the joiner has a real IP to TCP-connect to).
    CHECK(discovered != sf::IpAddress::None);
    CHECK(discovered != sf::IpAddress(0u));
    resetManager();
}

TEST_CASE("clientDiscoverHost times out when no host advertises the code")
{
    // No host is listening on this code -> the broadcast probe gets no
    // matching reply and the call must return false within the
    // requested timeout (we give it a generous upper bound to soak up
    // scheduler jitter).
    resetManager();
    auto& nm = NetworkManager::instance();
    sf::IpAddress out = sf::IpAddress::None;
    auto start = std::chrono::steady_clock::now();
    const bool ok = nm.clientDiscoverHost(1234, sf::seconds(1.f), &out);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    CHECK(!ok);
    CHECK(elapsed.count() < 3500);
    resetManager();
}

TEST_CASE("setExpectedPeers clamps to [1, kMaxJoiners]")
{
    resetManager();
    auto& nm = NetworkManager::instance();
    nm.setExpectedPeers(0);
    CHECK_EQ(nm.expectedPeers(), 1);
    nm.setExpectedPeers(-5);
    CHECK_EQ(nm.expectedPeers(), 1);
    nm.setExpectedPeers(NetworkManager::kMaxJoiners);
    CHECK_EQ(nm.expectedPeers(), NetworkManager::kMaxJoiners);
    nm.setExpectedPeers(NetworkManager::kMaxJoiners + 5);
    CHECK_EQ(nm.expectedPeers(), NetworkManager::kMaxJoiners);
    resetManager();
}

TEST_CASE("hostShouldStart is false until requestHostStart is called or lobby fills")
{
    resetManager();
    auto& nm = NetworkManager::instance();
    nm.setMode(NetworkManager::Mode::HOST);
    nm.setExpectedPeers(NetworkManager::kMaxJoiners);
    nm.setSeed(0);
    nm.setHost(sf::IpAddress::Any, static_cast<unsigned short>(kTestPort + 70));
    REQUIRE(nm.beginHostListening());

    // No peers, no request -> false.
    CHECK(!nm.hostShouldStart());
    CHECK(!nm.hostStartRequested());

    nm.requestHostStart();
    CHECK(nm.hostStartRequested());
    // Sticky -- still true even with zero joiners.
    CHECK(nm.hostShouldStart());

    resetManager();
}

TEST_CASE("host can force-start with zero joiners")
{
    resetManager();
    auto& nm = NetworkManager::instance();
    nm.setMode(NetworkManager::Mode::HOST);
    nm.setExpectedPeers(2);
    nm.setSeed(0);
    nm.setHost(sf::IpAddress::Any, static_cast<unsigned short>(kTestPort + 71));
    REQUIRE(nm.beginHostListening());
    CHECK_EQ(nm.acceptedPeers(), 0);

    nm.requestHostStart();
    CHECK(nm.hostShouldStart());
    // Total player count must reflect just the host -- nobody joined.
    CHECK_EQ(nm.totalPlayers(), 1);
    resetManager();
}

TEST_CASE("host can force-start after one joiner connects")
{
    resetManager();
    auto& nm = NetworkManager::instance();
    nm.setMode(NetworkManager::Mode::HOST);
    nm.setExpectedPeers(NetworkManager::kMaxJoiners);
    nm.setSeed(0);
    const unsigned short port = static_cast<unsigned short>(kTestPort + 72);
    nm.setHost(sf::IpAddress::Any, port);
    REQUIRE(nm.beginHostListening());

    FakeClient c1;
    c1.run(sf::IpAddress::LocalHost, port);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    bool gotOne = false;
    while (std::chrono::steady_clock::now() < deadline) {
        bool err = false;
        if (nm.pollAccept(&err)) { gotOne = true; break; }
        CHECK(!err);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(gotOne);
    c1.join();
    CHECK(c1.ok);
    CHECK_EQ(nm.acceptedPeers(), 1);
    CHECK_EQ(nm.totalPlayers(), 2);
    // hostShouldStart is still false because the lobby isn't capacity-
    // full and the host hasn't pressed START yet.
    CHECK(!nm.hostShouldStart());

    nm.requestHostStart();
    CHECK(nm.hostShouldStart());
    resetManager();
}

TEST_CASE("listener stays open after force-start so a late joiner can connect")
{
    resetManager();
    auto& nm = NetworkManager::instance();
    nm.setMode(NetworkManager::Mode::HOST);
    nm.setExpectedPeers(NetworkManager::kMaxJoiners);
    nm.setSeed(0);
    const unsigned short port = static_cast<unsigned short>(kTestPort + 73);
    nm.setHost(sf::IpAddress::Any, port);
    REQUIRE(nm.beginHostListening());

    // Simulate the host pressing START before anybody joined.
    nm.requestHostStart();
    CHECK(nm.hostShouldStart());
    CHECK_EQ(nm.acceptedPeers(), 0);

    // After the "game" started, a player should still be able to join.
    FakeClient late;
    late.run(sf::IpAddress::LocalHost, port);

    bool accepted = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        bool err = false;
        if (nm.pollAccept(&err)) { accepted = true; break; }
        CHECK(!err);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(accepted);
    late.join();
    CHECK(late.ok);
    // The late joiner is reported through the dedicated stream so the
    // gameplay layer can spawn their character.
    CHECK_EQ(nm.acceptedPeers(), 1);
    CHECK_EQ(nm.newlyJoinedSlot(), 1);
    CHECK_EQ(nm.newlyJoinedSlot(), -1); // only reported once

    // And the totalPlayers count grew when the late joiner came in.
    CHECK_EQ(nm.totalPlayers(), 2);
    resetManager();
}

TEST_CASE("pollAccept refuses to grow past kMaxJoiners")
{
    resetManager();
    auto& nm = NetworkManager::instance();
    nm.setMode(NetworkManager::Mode::HOST);
    nm.setExpectedPeers(NetworkManager::kMaxJoiners);
    nm.setSeed(0);
    const unsigned short port = static_cast<unsigned short>(kTestPort + 74);
    nm.setHost(sf::IpAddress::Any, port);
    REQUIRE(nm.beginHostListening());

    // Fill the lobby to capacity.
    std::vector<std::unique_ptr<FakeClient>> clients;
    for (int i = 0; i < NetworkManager::kMaxJoiners; ++i) {
        clients.emplace_back(new FakeClient());
        clients.back()->run(sf::IpAddress::LocalHost, port);
    }

    int accepted = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (accepted < NetworkManager::kMaxJoiners
           && std::chrono::steady_clock::now() < deadline) {
        bool err = false;
        if (nm.pollAccept(&err)) ++accepted;
        CHECK(!err);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK_EQ(accepted, NetworkManager::kMaxJoiners);
    CHECK_EQ(nm.acceptedPeers(), NetworkManager::kMaxJoiners);
    for (auto& c : clients) c->join();

    // Now try to push a fourth client through -- pollAccept must refuse
    // to grow past the cap even though the socket would otherwise be
    // accepted. Use a raw TcpSocket here instead of FakeClient: the host
    // will never send a handshake, so FakeClient's blocking receive()
    // would deadlock the joinable thread on shutdown.
    sf::TcpSocket extra;
    (void)extra.connect(sf::IpAddress::LocalHost, port, sf::seconds(1.f));

    bool grew = false;
    auto deadline2 = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline2) {
        bool err = false;
        if (nm.pollAccept(&err)) { grew = true; break; }
        CHECK(!err);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(!grew);
    CHECK_EQ(nm.acceptedPeers(), NetworkManager::kMaxJoiners);
    // The extra client's connect almost certainly succeeded at the TCP
    // layer (Windows happily ESTABLISHEs against a listening socket),
    // but it never received a handshake. That's the contract: the host
    // simply ignores connections beyond the cap.
    extra.disconnect();
    resetManager();
}

TEST_CASE("slotHealthSnapshot returns -1 by default and persists what is recorded")
{
    resetManager();
    auto& nm = NetworkManager::instance();
    CHECK_EQ(nm.slotHealthSnapshot(1), -1);
    CHECK_EQ(nm.slotHealthSnapshot(99), -1);

    nm.recordSlotHealth(1, 3);
    nm.recordSlotHealth(2, 5);
    CHECK_EQ(nm.slotHealthSnapshot(1), 3);
    CHECK_EQ(nm.slotHealthSnapshot(2), 5);

    // Overwriting a slot updates the value.
    nm.recordSlotHealth(1, 1);
    CHECK_EQ(nm.slotHealthSnapshot(1), 1);

    // disconnect() preserves the snapshot so a peer can come back; only
    // clearHealthSnapshots() wipes it.
    nm.disconnect();
    CHECK_EQ(nm.slotHealthSnapshot(1), 1);
    CHECK_EQ(nm.slotHealthSnapshot(2), 5);

    nm.clearHealthSnapshots();
    CHECK_EQ(nm.slotHealthSnapshot(1), -1);
    CHECK_EQ(nm.slotHealthSnapshot(2), -1);
    resetManager();
}

TEST_CASE("recordSlotHealth ignores zero/negative slot indices")
{
    resetManager();
    auto& nm = NetworkManager::instance();
    nm.recordSlotHealth(0, 7);
    nm.recordSlotHealth(-1, 8);
    CHECK_EQ(nm.slotHealthSnapshot(0), -1);
    CHECK_EQ(nm.slotHealthSnapshot(-1), -1);
    resetManager();
}

TEST_MAIN()
