#include "engine/NetworkManager.hpp"
#include "engine/EventManager.hpp"
#include "engine/Gamepad.hpp"
#include "engine/Env.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
// Magic numbers tag every packet so a stray byte stream from a wrong-version
// or wrong-protocol peer trips an early assertion instead of silently
// desynchronizing the simulation.
constexpr sf::Uint32 MAGIC_HANDSHAKE    = 0x48484E45u; // 'HHNE'
constexpr sf::Uint32 MAGIC_TICK_LOCAL   = 0x54494B21u; // 'TIK!'
constexpr sf::Uint32 MAGIC_TICK_BUNDLE  = 0x54424E44u; // 'TBND'
constexpr sf::Uint32 PROTOCOL_VERSION   = 1u;
// UDP discovery channel. The probe payload is
//   [MAGIC_DISCOVERY_PROBE][PROTOCOL_VERSION][roomCode]
// and the reply is
//   [MAGIC_DISCOVERY_REPLY][PROTOCOL_VERSION][roomCode][tcpPort]
// The reply's *source address* is what the client uses to TCP-connect,
// so the host doesn't have to enumerate its own interfaces.
constexpr sf::Uint32  MAGIC_DISCOVERY_PROBE = 0x48485151u; // 'HHQQ'
constexpr sf::Uint32  MAGIC_DISCOVERY_REPLY = 0x48485252u; // 'HHRR'
constexpr unsigned short DISCOVERY_PORT     = 53354;

void writeEdges(sf::Packet& pkt, const std::vector<NetworkManager::InputEdge>& edges)
{
    pkt << static_cast<sf::Uint32>(edges.size());
    for (const auto& e : edges) {
        pkt << e.type << e.slot << e.button;
    }
}

bool readEdges(sf::Packet& pkt, std::vector<NetworkManager::InputEdge>& out)
{
    sf::Uint32 count = 0;
    if (!(pkt >> count)) return false;
    out.reserve(out.size() + count);
    for (sf::Uint32 i = 0; i < count; ++i) {
        NetworkManager::InputEdge e;
        if (!(pkt >> e.type >> e.slot >> e.button)) return false;
        out.push_back(std::move(e));
    }
    return true;
}
} // namespace

// -------------------------- singleton -------------------------------------

NetworkManager& NetworkManager::instance()
{
    static NetworkManager inst;
    return inst;
}

// -------------------------- configuration ---------------------------------

void NetworkManager::configureFromEnv()
{
    std::string env;
    if (!Env::tryGet("HH_NET", &env) || env.empty()) {
        mode_ = Mode::OFFLINE;
        return;
    }

    std::vector<std::string> parts;
    {
        std::string token;
        std::istringstream iss(env);
        while (std::getline(iss, token, ':')) parts.push_back(token);
    }
    if (parts.empty()) {
        mode_ = Mode::OFFLINE;
        return;
    }

    if (parts[0] == "host" && parts.size() >= 2) {
        mode_ = Mode::HOST;
        int n = 1;
        try { n = std::stoi(parts[1]); } catch (...) { n = 1; }
        setExpectedPeers(n);
        if (parts.size() >= 3) {
            try { hostPort_ = static_cast<unsigned short>(std::stoi(parts[2])); }
            catch (...) { /* keep default */ }
        }
        // Host picks the seed once. Positive long so PutSeed treats it as
        // a deterministic seed (negative would re-derive from time()).
        seed_ = static_cast<long>(std::time(nullptr));
        if (seed_ <= 0) seed_ = 12345;
    }
    else if (parts[0] == "client" && parts.size() >= 2) {
        mode_ = Mode::CLIENT;
        hostAddress_ = sf::IpAddress(parts[1]);
        if (hostAddress_ == sf::IpAddress::None) {
            std::cerr << "[net] HH_NET client: bad host address '" << parts[1]
                      << "'. Falling back to OFFLINE." << std::endl;
            mode_ = Mode::OFFLINE;
            return;
        }
        if (parts.size() >= 3) {
            try { hostPort_ = static_cast<unsigned short>(std::stoi(parts[2])); }
            catch (...) { /* keep default */ }
        }
    }
    else {
        std::cerr << "[net] HH_NET unrecognized: '" << env
                  << "'. Expected 'host:N[:PORT]' or 'client:ADDR[:PORT]'."
                  << std::endl;
        mode_ = Mode::OFFLINE;
    }
}

// -------------------------- connect / handshake ---------------------------

bool NetworkManager::connectAndHandshake()
{
    if (mode_ == Mode::OFFLINE) return true;
    try {
        if (mode_ == Mode::HOST)  hostHandshake();
        else                       clientHandshake();
    }
    catch (const std::exception& ex) {
        std::cerr << "[net] handshake failed: " << ex.what()
                  << " -- falling back to OFFLINE." << std::endl;
        disconnect();
        mode_ = Mode::OFFLINE;
        return false;
    }
    return true;
}

void NetworkManager::hostHandshake()
{
    std::cout << "[net] HOST listening on port " << hostPort_
              << " for " << expectedPeers_ << " peer(s)..." << std::endl;
    if (listener_.listen(hostPort_) != sf::Socket::Done) {
        throw std::runtime_error("sf::TcpListener::listen() failed");
    }
    totalPlayers_ = 1 + expectedPeers_;
    localSlot_    = 0;

    // Accept clients one at a time, assigning slot 1..N-1 in accept order.
    for (int i = 0; i < expectedPeers_; ++i) {
        std::unique_ptr<sf::TcpSocket> sock(new sf::TcpSocket());
        if (listener_.accept(*sock) != sf::Socket::Done) {
            throw std::runtime_error("sf::TcpListener::accept() failed");
        }
        sock->setBlocking(true);
        const sf::Uint32 slot = static_cast<sf::Uint32>(i + 1);

        sf::Packet pkt;
        pkt << MAGIC_HANDSHAKE
            << PROTOCOL_VERSION
            << static_cast<sf::Uint32>(seed_)
            << static_cast<sf::Uint32>(totalPlayers_)
            << slot;
        if (sock->send(pkt) != sf::Socket::Done) {
            throw std::runtime_error("handshake send failed");
        }
        std::cout << "[net] HOST accepted peer at slot " << slot << std::endl;
        peerSockets_.push_back(std::move(sock));
    }
    std::cout << "[net] HOST handshake complete. seed=" << seed_
              << " totalPlayers=" << totalPlayers_ << std::endl;
}

void NetworkManager::clientHandshake()
{
    std::cout << "[net] CLIENT connecting to " << hostAddress_.toString()
              << ':' << hostPort_ << " ..." << std::endl;
    std::unique_ptr<sf::TcpSocket> sock(new sf::TcpSocket());
    if (sock->connect(hostAddress_, hostPort_) != sf::Socket::Done) {
        throw std::runtime_error("sf::TcpSocket::connect() failed");
    }
    sock->setBlocking(true);

    sf::Packet pkt;
    if (sock->receive(pkt) != sf::Socket::Done) {
        throw std::runtime_error("handshake receive failed");
    }

    sf::Uint32 magic = 0, version = 0, seedU = 0, total = 0, slot = 0;
    if (!(pkt >> magic >> version >> seedU >> total >> slot)) {
        throw std::runtime_error("handshake malformed");
    }
    if (magic != MAGIC_HANDSHAKE) {
        throw std::runtime_error("handshake magic mismatch");
    }
    if (version != PROTOCOL_VERSION) {
        throw std::runtime_error("handshake protocol version mismatch");
    }

    seed_         = static_cast<long>(seedU);
    totalPlayers_ = static_cast<int>(total);
    localSlot_    = static_cast<int>(slot);
    peerSockets_.push_back(std::move(sock));

    std::cout << "[net] CLIENT handshake complete. seed=" << seed_
              << " totalPlayers=" << totalPlayers_
              << " localSlot=" << localSlot_ << std::endl;
}

// -------------------------- non-blocking host setup -----------------------

bool NetworkManager::beginHostListening()
{
    // Caller is expected to have set mode_=HOST, hostPort_, expectedPeers_.
    // We initialize the same authoritative state hostHandshake() would set,
    // then open the listener in non-blocking mode so pollAccept() can be
    // driven from the UI thread one frame at a time.
    if (mode_ != Mode::HOST) {
        std::cerr << "[net] beginHostListening called outside HOST mode" << std::endl;
        return false;
    }
    if (expectedPeers_ < 1)              expectedPeers_ = 1;
    if (expectedPeers_ > kMaxJoiners)    expectedPeers_ = kMaxJoiners;
    if (listener_.listen(hostPort_) != sf::Socket::Done) {
        std::cerr << "[net] beginHostListening: listen(" << hostPort_ << ") failed" << std::endl;
        return false;
    }
    listener_.setBlocking(false);
    // Slot space starts at 1 (just the host) and grows as peers join.
    // Previously we baked in expectedPeers_ here, but joiners can drop
    // out before launch and the host can force-start with fewer than
    // expected, so totalPlayers_ must track the actual accept count.
    totalPlayers_ = 1;
    localSlot_    = 0;
    hostStartRequested_   = false;
    reportedJoinerCount_  = 0;
    // Host picks the seed once, matching the env-var path in hostHandshake().
    if (seed_ <= 0) {
        seed_ = static_cast<long>(std::time(nullptr));
        if (seed_ <= 0) seed_ = 12345;
    }
    // Generate a memorable 4-digit room code if none was set. Range
    // 1000..9999 keeps it exactly 4 digits with no leading-zero
    // ambiguity. Seeded off the wall clock + listener port so the same
    // build doesn't always pick the same number on quick reruns.
    if (roomCode_ < 1000 || roomCode_ > 9999) {
        unsigned seed = static_cast<unsigned>(std::time(nullptr))
                      ^ static_cast<unsigned>(hostPort_) * 2654435761u;
        std::srand(seed);
        roomCode_ = 1000 + (std::rand() % 9000);
    }
    // Open the UDP discovery socket so joiners on the same LAN can
    // resolve our IP from the room code. Non-fatal on bind failure --
    // the player can still relay an IP manually if they like.
    discoveryOpen_ = false;
    if (discoverySocket_.bind(DISCOVERY_PORT) == sf::Socket::Done) {
        discoverySocket_.setBlocking(false);
        discoveryOpen_ = true;
    }
    else {
        std::cerr << "[net] beginHostListening: UDP bind(" << DISCOVERY_PORT
                  << ") failed -- discovery disabled" << std::endl;
    }
    std::cout << "[net] HOST listening (non-blocking) on port " << hostPort_
              << " for " << expectedPeers_ << " peer(s); roomCode=" << roomCode_
              << std::endl;
    return true;
}

bool NetworkManager::pollAccept(bool* error)
{
    if (error) *error = false;
    if (mode_ != Mode::HOST) return false;
    // Always service the discovery socket -- even after all peers are in
    // -- so late joiners don't see a "ghost" timeout while we're still
    // technically up.
    serviceDiscovery();
    // Hard cap: never exceed kMaxJoiners actual remote players, no
    // matter what setExpectedPeers() was set to or how many force-start
    // late joiners we've taken on.
    if (static_cast<int>(peerSockets_.size()) >= kMaxJoiners) return false;

    std::unique_ptr<sf::TcpSocket> sock(new sf::TcpSocket());
    sf::Socket::Status st = listener_.accept(*sock);
    if (st == sf::Socket::NotReady) {
        return false; // no client pending; try again next tick
    }
    if (st != sf::Socket::Done) {
        std::cerr << "[net] pollAccept: accept failed (status=" << st << ")" << std::endl;
        if (error) *error = true;
        return false;
    }
    // We have a connected client; send the handshake while still blocking.
    sock->setBlocking(true);
    const sf::Uint32 slot = static_cast<sf::Uint32>(peerSockets_.size() + 1);
    // Bump the authoritative slot count BEFORE sending so the handshake
    // we hand the joiner already reflects them.
    totalPlayers_ = static_cast<int>(slot) + 1;
    sf::Packet pkt;
    pkt << MAGIC_HANDSHAKE
        << PROTOCOL_VERSION
        << static_cast<sf::Uint32>(seed_)
        << static_cast<sf::Uint32>(totalPlayers_)
        << slot;
    if (sock->send(pkt) != sf::Socket::Done) {
        std::cerr << "[net] pollAccept: handshake send to slot " << slot << " failed" << std::endl;
        if (error) *error = true;
        // Roll back the slot bump so the next attempt re-uses this slot.
        totalPlayers_ = static_cast<int>(slot);
        return false;
    }
    std::cout << "[net] HOST accepted peer at slot " << slot << std::endl;
    peerSockets_.push_back(std::move(sock));
    // The listener intentionally stays open so a player can join (or
    // rejoin) mid-game right up to the kMaxJoiners cap.
    return true;
}

bool NetworkManager::hostShouldStart() const
{
    if (mode_ != Mode::HOST) return false;
    // Either the host hit START, or the lobby filled to the cap. Both
    // conditions stay true until disconnect() resets us so the calling
    // screen can poll once per frame without missing the edge.
    if (hostStartRequested_) return true;
    return static_cast<int>(peerSockets_.size()) >= kMaxJoiners;
}

void NetworkManager::setExpectedPeers(int n)
{
    if (n < 1)              n = 1;
    if (n > kMaxJoiners)    n = kMaxJoiners;
    expectedPeers_ = n;
}

int NetworkManager::newlyJoinedSlot()
{
    if (mode_ != Mode::HOST) return -1;
    const int connected = static_cast<int>(peerSockets_.size());
    if (reportedJoinerCount_ >= connected) return -1;
    // Slot space is 1-based (host is slot 0). reportedJoinerCount_
    // tracks how many peers gameplay has already been told about; the
    // next un-reported slot is therefore reportedJoinerCount_ + 1.
    ++reportedJoinerCount_;
    return reportedJoinerCount_;
}

void NetworkManager::recordSlotHealth(int slot, int health)
{
    if (slot <= 0) return;
    slotHealth_[slot] = health;
}

int NetworkManager::slotHealthSnapshot(int slot) const
{
    auto it = slotHealth_.find(slot);
    if (it == slotHealth_.end()) return -1;
    return it->second;
}

bool NetworkManager::clientConnectWithTimeout(sf::Time timeout)
{
    if (mode_ != Mode::CLIENT) {
        std::cerr << "[net] clientConnectWithTimeout called outside CLIENT mode" << std::endl;
        return false;
    }
    std::cout << "[net] CLIENT connecting to " << hostAddress_.toString()
              << ':' << hostPort_ << " (timeout " << timeout.asSeconds() << "s)..." << std::endl;
    std::unique_ptr<sf::TcpSocket> sock(new sf::TcpSocket());
    if (sock->connect(hostAddress_, hostPort_, timeout) != sf::Socket::Done) {
        std::cerr << "[net] CLIENT connect timed out / failed" << std::endl;
        return false;
    }
    sock->setBlocking(true);
    sf::Packet pkt;
    if (sock->receive(pkt) != sf::Socket::Done) {
        std::cerr << "[net] CLIENT handshake receive failed" << std::endl;
        return false;
    }
    sf::Uint32 magic = 0, version = 0, seedU = 0, total = 0, slot = 0;
    if (!(pkt >> magic >> version >> seedU >> total >> slot)
        || magic != MAGIC_HANDSHAKE
        || version != PROTOCOL_VERSION) {
        std::cerr << "[net] CLIENT handshake malformed / version mismatch" << std::endl;
        return false;
    }
    seed_         = static_cast<long>(seedU);
    totalPlayers_ = static_cast<int>(total);
    localSlot_    = static_cast<int>(slot);
    peerSockets_.push_back(std::move(sock));
    std::cout << "[net] CLIENT handshake complete. seed=" << seed_
              << " totalPlayers=" << totalPlayers_
              << " localSlot=" << localSlot_ << std::endl;
    return true;
}

void NetworkManager::disconnect()
{
    for (auto& sock : peerSockets_) {
        if (sock) sock->disconnect();
    }
    peerSockets_.clear();
    listener_.close();
    if (discoveryOpen_) {
        discoverySocket_.unbind();
        discoveryOpen_ = false;
    }
    // Reset all per-session host bookkeeping so the next round starts
    // clean. Health snapshots are intentionally NOT cleared here: a
    // "player drops their connection mid-game" event must preserve HP
    // for the rejoin path. Use clearHealthSnapshots() at end-of-round.
    hostStartRequested_   = false;
    reportedJoinerCount_  = 0;
    totalPlayers_         = 1;
    localSlot_            = 0;
    tickIndex_            = 0;
}

void NetworkManager::serviceDiscovery()
{
    if (!discoveryOpen_) return;
    // Drain every datagram waiting on the socket -- a tight burst of
    // probes from multiple clients should all get answered in one tick.
    for (;;) {
        sf::Packet     pkt;
        sf::IpAddress  sender;
        unsigned short senderPort = 0;
        const sf::Socket::Status st = discoverySocket_.receive(pkt, sender, senderPort);
        if (st == sf::Socket::NotReady) return;
        if (st != sf::Socket::Done)     return; // transient error; try again next tick
        sf::Uint32 magic = 0, version = 0;
        sf::Int32  code  = 0;
        if (!(pkt >> magic >> version >> code))               continue;
        if (magic != MAGIC_DISCOVERY_PROBE)                   continue;
        if (version != PROTOCOL_VERSION)                      continue;
        if (code != static_cast<sf::Int32>(roomCode_))        continue;
        // Reply: same room code so the client can sanity-check the
        // datagram came from a matching host (defensive against another
        // game on the same port). Source IP/port of the reply IS what
        // the client will TCP-connect to, so we don't have to send our
        // address.
        sf::Packet reply;
        reply << MAGIC_DISCOVERY_REPLY
              << PROTOCOL_VERSION
              << static_cast<sf::Int32>(roomCode_)
              << static_cast<sf::Uint16>(hostPort_);
        discoverySocket_.send(reply, sender, senderPort);
    }
}

bool NetworkManager::clientDiscoverHost(int code, sf::Time timeout, sf::IpAddress* outIp)
{
    // Open a short-lived UDP socket on an OS-assigned port, broadcast a
    // probe carrying the room code, then poll for a matching reply
    // until the timeout elapses. Returns the responder's source address
    // (the host's LAN IP) via *outIp.
    if (outIp == nullptr) return false;
    sf::UdpSocket socket;
    if (socket.bind(sf::Socket::AnyPort) != sf::Socket::Done) {
        std::cerr << "[net] clientDiscoverHost: bind(AnyPort) failed" << std::endl;
        return false;
    }
    socket.setBlocking(false);
    sf::Packet probe;
    probe << MAGIC_DISCOVERY_PROBE
          << PROTOCOL_VERSION
          << static_cast<sf::Int32>(code);
    // Broadcast on the local subnet; also send to 127.0.0.1 so a loopback
    // host on the same machine answers without depending on a NIC
    // broadcast route (Windows often blocks 255.255.255.255 from a
    // socket bound to an OS-assigned port).
    socket.send(probe, sf::IpAddress::Broadcast, DISCOVERY_PORT);
    socket.send(probe, sf::IpAddress::LocalHost, DISCOVERY_PORT);

    sf::Clock clock;
    while (clock.getElapsedTime() < timeout) {
        sf::Packet     reply;
        sf::IpAddress  sender;
        unsigned short senderPort = 0;
        const sf::Socket::Status st = socket.receive(reply, sender, senderPort);
        if (st == sf::Socket::Done) {
            sf::Uint32 magic = 0, version = 0;
            sf::Int32  rcode = 0;
            sf::Uint16 rport = 0;
            if (!(reply >> magic >> version >> rcode >> rport)) continue;
            if (magic != MAGIC_DISCOVERY_REPLY)                  continue;
            if (version != PROTOCOL_VERSION)                     continue;
            if (rcode != static_cast<sf::Int32>(code))           continue;
            *outIp = sender;
            // Caller's TCP-connect target already lives on hostPort_; we
            // honor whatever the reply advertised so the host can
            // override it later if it ever needs to.
            hostPort_ = rport;
            socket.unbind();
            return true;
        }
        // Throttle the poll to ~5ms so we don't burn a core.
        sf::sleep(sf::milliseconds(5));
    }
    socket.unbind();
    return false;
}

void NetworkManager::downgradeToOffline(const char* reason)
{
    std::cerr << "[net] downgrading to OFFLINE: " << reason << std::endl;
    disconnect();
    mode_ = Mode::OFFLINE;
    localBuffer_.clear();
}

// -------------------------- input capture ---------------------------------

bool NetworkManager::interceptLocalGamepad(const std::string& button, bool isPressed)
{
    if (mode_ == Mode::OFFLINE) return false;

    InputEdge edge;
    edge.type   = isPressed ? 0u : 1u;
    edge.slot   = static_cast<uint8_t>(localSlot_);
    edge.button = button;
    localBuffer_.push_back(std::move(edge));
    return true;
}

// -------------------------- tick exchange ---------------------------------

void NetworkManager::tickStep()
{
    if (mode_ == Mode::OFFLINE) return;
    if (mode_ == Mode::HOST) hostTick();
    else                      clientTick();
    ++tickIndex_;
}

void NetworkManager::hostTick()
{
    // Start with own (host-slot) input for this tick.
    std::vector<InputEdge> merged = std::move(localBuffer_);
    localBuffer_.clear();

    // Receive one TIK from each client (blocking) and append its edges.
    for (std::size_t i = 0; i < peerSockets_.size(); ++i) {
        auto& sock = peerSockets_[i];
        sf::Packet pkt;
        if (sock->receive(pkt) != sf::Socket::Done) {
            downgradeToOffline("HOST: client recv failed");
            return;
        }
        sf::Uint32 magic = 0, tick = 0, claimedSlot = 0;
        if (!(pkt >> magic >> tick >> claimedSlot) || magic != MAGIC_TICK_LOCAL) {
            std::cerr << "[net] HOST: bad TIK packet from slot " << (i + 1)
                      << ", dropping its inputs this tick." << std::endl;
            continue;
        }
        std::vector<InputEdge> peerEdges;
        if (!readEdges(pkt, peerEdges)) {
            std::cerr << "[net] HOST: malformed TIK edges from slot " << (i + 1)
                      << "." << std::endl;
            continue;
        }
        // Server-authoritative slot id: rewrite from socket position rather
        // than trusting the client's self-reported slot.
        const uint8_t peerSlot = static_cast<uint8_t>(i + 1);
        for (auto& e : peerEdges) {
            e.slot = peerSlot;
            merged.push_back(std::move(e));
        }
    }

    // Broadcast TBND to every client.
    sf::Packet bundle;
    bundle << MAGIC_TICK_BUNDLE << static_cast<sf::Uint32>(tickIndex_);
    writeEdges(bundle, merged);
    for (auto& sock : peerSockets_) {
        if (sock->send(bundle) != sf::Socket::Done) {
            downgradeToOffline("HOST: client send failed");
            return;
        }
    }

    // Apply the merged set locally so the host sees its own inputs at the
    // same tick the clients will.
    emitMergedEventsToBus(merged);
}

void NetworkManager::clientTick()
{
    // Ship own buffer to host.
    sf::Packet tikPkt;
    tikPkt << MAGIC_TICK_LOCAL
           << static_cast<sf::Uint32>(tickIndex_)
           << static_cast<sf::Uint32>(localSlot_);
    writeEdges(tikPkt, localBuffer_);
    localBuffer_.clear();

    auto& host = peerSockets_.front();
    if (host->send(tikPkt) != sf::Socket::Done) {
        downgradeToOffline("CLIENT: send to host failed");
        return;
    }

    // Block on the host's merged bundle for this tick.
    sf::Packet bundle;
    if (host->receive(bundle) != sf::Socket::Done) {
        downgradeToOffline("CLIENT: recv from host failed");
        return;
    }
    sf::Uint32 magic = 0, tick = 0;
    if (!(bundle >> magic >> tick) || magic != MAGIC_TICK_BUNDLE) {
        std::cerr << "[net] CLIENT: bad TBND packet, skipping tick." << std::endl;
        return;
    }
    std::vector<InputEdge> merged;
    if (!readEdges(bundle, merged)) {
        std::cerr << "[net] CLIENT: malformed TBND edges." << std::endl;
        return;
    }
    emitMergedEventsToBus(merged);
}

void NetworkManager::emitMergedEventsToBus(const std::vector<InputEdge>& edges)
{
    for (const auto& e : edges) {
        auto ev = std::make_shared<GamepadEvent>();
        ev->button = e.button;
        ev->index  = static_cast<int>(e.slot);     // synthetic slot == gamepadIndex
        ev->type   = (e.type == 0u) ? GamepadEvent::TYPE::PRESSED
                                    : GamepadEvent::TYPE::RELEASED;
        Events::queueEvent("gamepad_event", ev);
    }
}
