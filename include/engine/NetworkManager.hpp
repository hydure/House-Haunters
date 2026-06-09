#ifndef NETWORK_MANAGER_HPP
#define NETWORK_MANAGER_HPP
/////////////////////////////////////////////////
// NetworkManager.hpp
//
// Opt-in TCP lockstep backbone for House Haunters.
//
// Topology:
//   * One peer runs as HOST, the rest as CLIENT.
//   * On startup each peer parses the HH_NET env var:
//        HH_NET=host:NUM_PEERS[:PORT]      e.g. HH_NET=host:1:53353
//        HH_NET=client:HOST_ADDR[:PORT]    e.g. HH_NET=client:192.168.1.42
//     With no HH_NET set the manager stays in OFFLINE mode and every
//     other call is a no-op -- the offline (couch co-op) build path is
//     unaffected.
//
// Handshake (host -> each client, single packet):
//     [u32 magic='HHNE'][u32 version][u32 seed]
//     [u32 totalPlayers][u32 yourSlotIndex]
//   * seed is broadcast so PlantSeeds(seed) gives every peer the same
//     RNG stream (item #4 -- deterministic simulation -- is the
//     prerequisite for this to actually stay in sync).
//   * Slot 0 is the host; clients are assigned 1..N-1 in accept order.
//
// Per-tick exchange (called from GameEngine::update):
//   client -> host:
//     [u32 magic='TIK!'][u32 tickIndex][u32 mySlot]
//     [u32 numEdges] (u8 type, u8 slot, std::string button)+
//   host -> all clients:
//     [u32 magic='TBND'][u32 tickIndex]
//     [u32 numEdges] (u8 type, u8 slot, std::string button)+
//   The host re-stamps each peer's slot field from the socket position
//   (not the packet body) so a misbehaving client can't impersonate
//   another player.
//
// Input integration:
//   * Gamepad::dispatchButtonState consults interceptLocalGamepad(...).
//     In OFFLINE mode it returns false and the existing local-queue
//     path runs unchanged. In HOST/CLIENT mode it returns true and the
//     edge is buffered for the current tick instead of being queued
//     directly -- it'll show up locally only after it has round-tripped
//     through the host (the lockstep input-delay cost).
//   * tickStep() drains the buffer, exchanges packets, and re-queues
//     the merged bundle on the local Events bus tagged with each
//     player's slot id (which is what PlayerView already filters on).
//
// Known scope limits (intentional v1):
//   * No reconnect, no rollback, no client prediction. A dropped peer
//     downgrades the manager to OFFLINE and the game keeps running
//     locally with whatever state it has.
//   * Per-tick wait is blocking; the slowest peer dictates the frame
//     rate. That's correct lockstep -- input-delay smoothing is left
//     to a follow-up if needed.
//   * Character selection sync is bypassed: clients use a deterministic
//     default char_map (slot N -> CHARACTER(N % 4)). A networked
//     character-select screen is its own UX task.
/////////////////////////////////////////////////

#include <SFML/Network.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class NetworkManager
{
public:
    enum class Mode { OFFLINE, HOST, CLIENT };

    // Hard cap on remote joiners. Host counts itself separately, so the
    // game tops out at kMaxJoiners + 1 = 4 players (matches the local
    // couch-co-op flow and the four character portraits on the team
    // select screen).
    static constexpr int kMaxJoiners = 3;

    // One input edge for a single player slot in a single tick.
    struct InputEdge {
        uint8_t     type;   // 0=PRESSED, 1=RELEASED
        uint8_t     slot;   // synthetic player slot (0..N-1), host-authoritative
        std::string button; // e.g. "A", "UP"
    };

    static NetworkManager& instance();

    // Parse HH_NET. Safe to call once at startup. Defaults to OFFLINE.
    void configureFromEnv();

    void setMode(Mode m)                                  { mode_ = m; }
    void setHost(const sf::IpAddress& addr, unsigned short port)
                                                          { hostAddress_ = addr; hostPort_ = port; }
    // Capacity the host advertises (1..kMaxJoiners). The host may force-
    // start with fewer peers actually connected; this is just an upper
    // bound on how many joiners we'll accept before saying "full".
    void setExpectedPeers(int n);
    void setSeed(long s)                                  { seed_ = s; }

    Mode mode() const                                     { return mode_; }
    bool isOffline() const                                { return mode_ == Mode::OFFLINE; }
    bool isNetworked() const                              { return mode_ != Mode::OFFLINE; }
    int  totalPlayers() const                             { return totalPlayers_; }
    int  localSlot() const                                { return localSlot_; }
    long seed() const                                     { return seed_; }
    int  expectedPeers() const                            { return expectedPeers_; }

    // Open sockets, run handshake. Returns false on failure (and downgrades
    // to OFFLINE). Blocks until all clients connect (host) or the host
    // responds (client).
    bool connectAndHandshake();

    // ---- non-blocking host setup, for the in-game Host menu ---------
    // Opens the TCP listener in non-blocking mode and primes the host
    // state (seed, totalPlayers, localSlot=0). Returns false if the
    // port could not be bound. Also opens the UDP discovery socket so
    // joiners on the LAN can resolve our IP from the 4-digit room code
    // without the user having to type out an address.
    bool beginHostListening();
    // Accepts at most one waiting client per call. Sends the handshake
    // packet for each newly-accepted client. Returns true when a new
    // peer was just accepted (false otherwise: no client pending, or
    // we're already at the kMaxJoiners cap). The listener stays open
    // until disconnect() is called, so this same entry point also
    // services mid-game joiners.
    // Sets *error to true if the underlying socket call failed.
    bool pollAccept(bool* error = nullptr);
    int  acceptedPeers() const { return static_cast<int>(peerSockets_.size()); }

    // -- Host start control ------------------------------------------
    // HostScreen calls requestHostStart() when the user hits A/START to
    // launch the game with however many peers have joined so far.
    // hostShouldStart() returns true once either:
    //   * the host explicitly requested a start, OR
    //   * the lobby has filled to the kMaxJoiners cap (auto-start).
    // Both conditions are sticky: once true, they stay true until
    // disconnect() resets the manager. Caller polls this every frame
    // instead of waiting on pollAccept's return value -- a host can
    // now legitimately start with 0 joiners.
    void requestHostStart() { hostStartRequested_ = true; }
    bool hostStartRequested() const { return hostStartRequested_; }
    bool hostShouldStart() const;

    // -- Late join book-keeping --------------------------------------
    // GameplayScreen polls newlyJoinedSlot() once per frame to discover
    // any peer that connected AFTER the game started. Returns the
    // 1-based player number for that peer, or -1 when no late joiner is
    // pending. Each accepted peer is reported exactly once.
    int newlyJoinedSlot();

    // Per-slot health snapshot used to restore a player's HP when they
    // reconnect. Slot is the 1-based player number; -1 == "no record".
    void recordSlotHealth(int slot, int health);
    int  slotHealthSnapshot(int slot) const;
    void clearHealthSnapshots() { slotHealth_.clear(); }

    // 4-digit room code shown on the host screen. Generated inside
    // beginHostListening() if the caller hasn't set one explicitly; the
    // host UI reads it back to draw the big code label. setRoomCode is
    // mainly for tests / determinism.
    void setRoomCode(int code) { roomCode_ = code; }
    int  roomCode() const      { return roomCode_; }

    // ---- client-side connect with an explicit timeout --------------
    // Broadcast a UDP discovery probe carrying the room code on the LAN
    // and wait up to `timeout` for a host on this code to answer. On
    // success returns true and writes the discovered host address into
    // *outIp; on timeout / parse failure returns false. Pure I/O --
    // does NOT mutate the OFFLINE/HOST/CLIENT mode.
    bool clientDiscoverHost(int code, sf::Time timeout, sf::IpAddress* outIp);

    // Used by the JoinScreen so a wrong code times out in seconds
    // instead of hanging the UI for ~minute-long OS TCP retries.
    bool clientConnectWithTimeout(sf::Time timeout);

    void disconnect();

    // Called by Gamepad::dispatchButtonState BEFORE it queues a local event.
    // Returns true in HOST/CLIENT mode (caller skips the local queue path
    // since the edge is now owned by NetworkManager). Returns false in
    // OFFLINE mode so the caller falls through to the normal local path.
    bool interceptLocalGamepad(const std::string& button, bool isPressed);

    // Per-tick lockstep step. Called from GameEngine::update once per
    // fixed timestep. In OFFLINE mode it's a no-op; in HOST/CLIENT mode
    // it sends the local buffer, blocks until peer input has been merged,
    // and queues the merged events on the Events bus tagged with slot ids.
    void tickStep();

private:
    NetworkManager() = default;
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    void hostHandshake();
    void clientHandshake();
    void hostTick();
    void clientTick();
    void emitMergedEventsToBus(const std::vector<InputEdge>& edges);
    void downgradeToOffline(const char* reason);
    // Non-blocking poll of the UDP discovery socket. Replies to any
    // probe whose payload matches our room code. Safe to call every
    // tick; returns immediately when the socket is closed or no datagram
    // is waiting.
    void serviceDiscovery();

    Mode           mode_           = Mode::OFFLINE;
    sf::IpAddress  hostAddress_    = sf::IpAddress::None;
    unsigned short hostPort_       = 53353;
    int            expectedPeers_  = 0;     // HOST: peer-cap advertised (1..kMaxJoiners)
    int            totalPlayers_   = 1;     // size of slot space (1..N)
    int            localSlot_      = 0;     // host=0, clients=1..N-1
    long           seed_           = 0;
    uint32_t       tickIndex_      = 0;
    // 4-digit room code (1000..9999). 0 means "unset". Generated by
    // beginHostListening() when needed. Clients carry it in their UDP
    // probe so two unrelated rooms on the same LAN don't crosstalk.
    int            roomCode_       = 0;
    // Host-side: sticky flag set by requestHostStart(). Lets the host
    // force-launch gameplay with whatever number of peers are currently
    // connected (even zero). hostShouldStart() OR's this together with
    // "lobby full" so the existing auto-start-when-full behavior still
    // works.
    bool           hostStartRequested_ = false;
    // Number of peers reported to the gameplay layer so far. Anything
    // accepted beyond this is a "late joiner" and gets surfaced through
    // newlyJoinedSlot(). reset to 0 inside disconnect().
    int            reportedJoinerCount_ = 0;
    // Per-slot last-known health, keyed by 1-based player number.
    // Populated by recordSlotHealth() (GameplayScreen pushes the value
    // every frame) and consumed by slotHealthSnapshot() when a rejoiner
    // is spawned.
    std::unordered_map<int, int> slotHealth_;

    sf::TcpListener listener_;
    // HOST: UDP socket bound to the discovery port that answers join
    // probes carrying our roomCode_. CLIENT: unused.
    sf::UdpSocket   discoverySocket_;
    bool            discoveryOpen_  = false;
    // HOST: one socket per connected client, peerSockets_[i] == slot (i+1).
    // CLIENT: exactly one socket pointing at the host.
    std::vector< std::unique_ptr<sf::TcpSocket> > peerSockets_;

    // Local-side edges produced this tick (drained by tickStep).
    std::vector<InputEdge> localBuffer_;
};

#endif
