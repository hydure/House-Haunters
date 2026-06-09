#include "game/screens/HostScreen.hpp"
#include "game/JoinCode.hpp"
#include "engine/Paths.hpp"
#include "engine/NetworkManager.hpp"
#include "engine/Random.hpp"
#include <SFML/Network.hpp>
#include <sstream>

namespace {

// HostScreen formats the LAN IP + 8-char hex JOIN CODE; the actual
// encode lives in JoinCode.hpp so the JoinScreen path can decode the
// same shape (and so it is unit-testable in isolation).

} // namespace

void HostScreen::init()
{
    // RAII subscription -- dropped automatically when the engine leaves us.
    this->subscribe("gamepad_event", [this](base_event_type e) {
        auto gpe = dynamic_cast< GamepadEvent& >(*e);
        this->onGamepadEvent(gpe);
    });
    // ESC on keyboard cancels back to the title (matches the JoinScreen
    // convention so the two screens behave identically).
    this->subscribe("key_input", [this](base_event_type e) {
        auto& se = dynamic_cast< Event<std::string>& >(*e);
        this->onKeyInput(se.data);
    });

    background.setSize(sf::Vector2f(720.f, 480.f));
    background.setFillColor(sf::Color(10, 5, 20, 235));

    const sf::Font& font = *ResourceManager::getFont(
        Paths::resource("fonts/Underdog-Regular.ttf"));

    title.setFont(font);
    title.setString("HOST A GAME");
    title.setCharacterSize(36);
    title.setFillColor(sf::Color::White);
    title.setStyle(sf::Text::Bold);
    {
        sf::FloatRect b = title.getLocalBounds();
        title.setOrigin(b.left + b.width / 2.f, 0.f);
        title.setPosition(720.f / 2.f, 30.f);
    }

    ipLabel.setFont(font);
    ipLabel.setCharacterSize(20);
    ipLabel.setFillColor(sf::Color(220, 220, 220));

    codeLabel.setFont(font);
    // Slightly smaller than before (40 -> 30) because the new decimal
    // code is longer than the old hex one; 30 fits comfortably at 720
    // wide with margins on both sides.
    codeLabel.setCharacterSize(30);
    codeLabel.setFillColor(sf::Color(255, 220, 80));
    codeLabel.setStyle(sf::Text::Bold);

    status.setFont(font);
    status.setCharacterSize(18);
    status.setFillColor(sf::Color(180, 220, 255));

    footer.setFont(font);
    footer.setCharacterSize(14);
    footer.setFillColor(sf::Color(200, 200, 200));
    footer.setStyle(sf::Text::Italic);
    footer.setString("LEFT/RIGHT change cap   A/START begin   B/X cancel");
    {
        sf::FloatRect b = footer.getLocalBounds();
        footer.setOrigin(b.left + b.width / 2.f, 0.f);
        footer.setPosition(720.f / 2.f, 480.f - 28.f);
    }

    // Default to a single remote peer (2-player session).
    expectedPeers = 1;
    listening     = false;
    done          = false;
    errored       = false;
    ready         = false;

    auto& net = NetworkManager::instance();
    net.disconnect();                // clear any leftover sockets
    net.setMode(NetworkManager::Mode::HOST);
    net.setExpectedPeers(expectedPeers);
    // Force NM to pick a fresh seed once beginHostListening runs.
    net.setSeed(0);
    if (!net.beginHostListening()) {
        errored = true;
    }
    else {
        listening = true;
    }
    refreshLabels();
}

void HostScreen::refreshLabels()
{
    // Room code is generated inside NetworkManager::beginHostListening so
    // we just read it back here. LAN IP is still shown as a courtesy
    // for power-users who already know the IP of the host machine, but
    // the CODE line is now the canonical join token (NetworkManager's
    // UDP discovery turns it back into an IP on the joiner side).
    const sf::IpAddress local = sf::IpAddress::getLocalAddress();
    auto& net = NetworkManager::instance();
    const int          code  = net.roomCode();

    {
        std::ostringstream oss;
        oss << "LAN IP: " << local.toString();
        ipLabel.setString(oss.str());
        sf::FloatRect b = ipLabel.getLocalBounds();
        ipLabel.setOrigin(b.left + b.width / 2.f, 0.f);
        ipLabel.setPosition(720.f / 2.f, 110.f);
    }
    {
        // 4-digit zero-padded code. Bigger character size than before
        // because there's much less to draw -- nothing else competes for
        // horizontal space on this line.
        codeLabel.setString(std::string("CODE: ") + JoinCode::encode(code));
        sf::FloatRect b = codeLabel.getLocalBounds();
        codeLabel.setOrigin(b.left + b.width / 2.f, 0.f);
        codeLabel.setPosition(720.f / 2.f, 160.f);
    }
    {
        std::ostringstream oss;
        const int connected = net.acceptedPeers();
        if (errored) {
            oss << "Failed to start hosting (port " << 53353
                << " in use?).  Press B/X to go back.";
        }
        else if (done) {
            oss << "Starting with " << connected
                << " joiner(s) (" << (connected + 1) << " player(s) total)...";
        }
        else {
            oss << connected << " joiner(s) connected (cap " << expectedPeers
                << ").  Press A/START to begin now -- more can join mid-game.";
        }
        status.setString(oss.str());
        sf::FloatRect b = status.getLocalBounds();
        status.setOrigin(b.left + b.width / 2.f, 0.f);
        status.setPosition(720.f / 2.f, 260.f);
    }
    {
        std::ostringstream oss;
        oss << "Cap = " << expectedPeers << " remote peer(s)   "
            << "LEFT/RIGHT change cap   A/START begin   B/X cancel";
        footer.setString(oss.str());
        sf::FloatRect b = footer.getLocalBounds();
        footer.setOrigin(b.left + b.width / 2.f, 0.f);
        footer.setPosition(720.f / 2.f, 480.f - 28.f);
    }
}

void HostScreen::onUpdate(float /*dt*/)
{
    if (!ready) {
        ready = true;
    }
    if (!listening || done || errored) return;

    auto& net = NetworkManager::instance();
    bool err = false;
    const bool justAccepted = net.pollAccept(&err);
    if (err) {
        errored = true;
        refreshLabels();
        return;
    }
    // hostShouldStart() is the single source of truth for "go now":
    // it returns true when the host hit A/START OR the lobby filled to
    // the kMaxJoiners cap. pollAccept no longer closes the listener at
    // expected-peer count -- joiners are allowed to keep arriving up to
    // the cap, both before and after launch.
    if (net.hostShouldStart()) {
        done = true;
        refreshLabels();
        launchGameplay();
        return;
    }
    // Refresh status counter only when it would actually change.
    if (justAccepted) {
        refreshLabels();
    }
}

void HostScreen::launchGameplay()
{
    auto& net = NetworkManager::instance();

    // Mirror the env-var bootstrap in HouseHaunters.cpp so the gameplay
    // screen sees a fully-populated Config.
    PlantSeeds(net.seed());
    config->num_players = net.totalPlayers();
    config->player_map.clear();
    config->char_map.clear();
    for (int slot = 0; slot < config->num_players; ++slot) {
        config->player_map[slot]   = slot + 1;
        config->char_map[slot + 1] =
            static_cast<Config::CHARACTER>(slot % 4);
    }
    auto evt = std::make_shared< Event<std::string> >("GamePlay");
    Events::triggerEvent("change_screen", evt);
}

void HostScreen::cancelToTitle()
{
    auto& net = NetworkManager::instance();
    net.disconnect();
    net.setMode(NetworkManager::Mode::OFFLINE);
    auto evt = std::make_shared< Event<std::string> >("Title");
    Events::triggerEvent("change_screen", evt);
}

void HostScreen::onGamepadEvent(GamepadEvent e)
{
    if (!ready) return;
    if (done)  return; // ignore input once we've launched gameplay
    if (e.type != GamepadEvent::RELEASED) return;

    auto& net = NetworkManager::instance();

    // A / START / ENTER launches the game with whatever peers are in.
    // Pressing while still in the lobby is the headline new behavior
    // for this iteration -- the host no longer has to wait for the
    // capacity to fill before everyone can play.
    if (!errored && (e.button == "A" || e.button == "START")) {
        net.requestHostStart();
        return;
    }

    // Allow LEFT/RIGHT to tweak the cap *before* a client connects --
    // once anyone is in, totalPlayers_ is already non-trivial and
    // tearing the listener down would drop them.
    if (!errored && net.acceptedPeers() == 0
        && (e.button == "LEFT" || e.button == "RIGHT"))
    {
        const int delta = (e.button == "RIGHT") ? +1 : -1;
        int newPeers = expectedPeers + delta;
        if (newPeers < 1) newPeers = NetworkManager::kMaxJoiners;
        if (newPeers > NetworkManager::kMaxJoiners) newPeers = 1;
        if (newPeers != expectedPeers) {
            expectedPeers = newPeers;
            // Tear down & restart the listener so the cap takes effect.
            net.disconnect();
            net.setMode(NetworkManager::Mode::HOST);
            net.setExpectedPeers(expectedPeers);
            net.setSeed(0);
            listening = false;
            errored   = false;
            if (net.beginHostListening()) {
                listening = true;
            }
            else {
                errored = true;
            }
        }
        refreshLabels();
        return;
    }

    if (e.button == "B" || e.button == "MENU") {
        cancelToTitle();
        return;
    }
}

void HostScreen::onKeyInput(const std::string& key)
{
    if (!ready) return;
    if (done)  return;
    if (errored) {
        // Esc still cancels even while erroring.
        if (key == "ESC") cancelToTitle();
        return;
    }
    if (key == "ENTER") {
        // Keyboard equivalent of A/START.
        NetworkManager::instance().requestHostStart();
        return;
    }
    if (key == "ESC") {
        cancelToTitle();
    }
}

void HostScreen::onDraw(sf::RenderTarget& ctx, sf::RenderStates /*states*/) const
{
    ctx.draw(background);
    ctx.draw(title);
    ctx.draw(ipLabel);
    ctx.draw(codeLabel);
    ctx.draw(status);
    ctx.draw(footer);
}
