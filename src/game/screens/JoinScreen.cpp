#include "game/screens/JoinScreen.hpp"
#include "game/JoinCode.hpp"
#include "engine/Paths.hpp"
#include "engine/NetworkManager.hpp"
#include "engine/Random.hpp"
#include <SFML/Network.hpp>
#include <sstream>

namespace {

// Layout: 4 digit slots laid out in a row, no separators. The widths
// below were tuned by eye for the Underdog-Regular font at 48 pt; tweak
// if you change the font or size.
constexpr float kSlotsY      = 200.f;
constexpr float kDigitSpan   = 48.f;   // width per digit slot (a bit roomier
                                       // than the old 12-digit layout since
                                       // we only have 4 slots to fill now)
constexpr float kSlotsTotalW = kDigitSpan * JoinCode::kCodeLen;
constexpr float kSlotsLeft   = (720.f - kSlotsTotalW) / 2.f;

// Convert a slot index 0..3 to the x-center of that digit's text.
float slotCenterX(int slotIdx)
{
    return kSlotsLeft + (slotIdx + 0.5f) * kDigitSpan;
}

} // namespace

void JoinScreen::init()
{
    // Reset per-entry state so re-entering the screen after a failed
    // attempt or cancel starts from a clean slate.
    digits.fill(0);
    cursor = 0;
    busy   = false;
    failed = false;
    done   = false;
    ready  = false;

    this->subscribe("gamepad_event", [this](base_event_type e) {
        auto gpe = dynamic_cast< GamepadEvent& >(*e);
        this->onGamepadEvent(gpe);
    });
    // Direct keystroke pipe -- lets the player type the numeric code on
    // the keyboard without going through the d-pad cycle.
    this->subscribe("key_input", [this](base_event_type e) {
        auto& se = dynamic_cast< Event<std::string>& >(*e);
        this->onKeyInput(se.data);
    });

    background.setSize(sf::Vector2f(720.f, 480.f));
    background.setFillColor(sf::Color(10, 5, 20, 235));

    const sf::Font& font = *ResourceManager::getFont(
        Paths::resource("fonts/Underdog-Regular.ttf"));

    title.setFont(font);
    title.setString("JOIN A GAME");
    title.setCharacterSize(36);
    title.setFillColor(sf::Color::White);
    title.setStyle(sf::Text::Bold);
    {
        sf::FloatRect b = title.getLocalBounds();
        title.setOrigin(b.left + b.width / 2.f, 0.f);
        title.setPosition(720.f / 2.f, 30.f);
    }

    prompt.setFont(font);
    prompt.setString("Type the 4-digit room code shown on the host's screen:");
    prompt.setCharacterSize(16);
    prompt.setFillColor(sf::Color(220, 220, 220));
    {
        sf::FloatRect b = prompt.getLocalBounds();
        prompt.setOrigin(b.left + b.width / 2.f, 0.f);
        prompt.setPosition(720.f / 2.f, 130.f);
    }

    for (int i = 0; i < JoinCode::kCodeLen; ++i) {
        slots[i].setFont(font);
        // Bigger glyphs now that we only have 4 slots to fill.
        slots[i].setCharacterSize(56);
        slots[i].setStyle(sf::Text::Bold);
        slots[i].setString(std::string(1, JoinCode::digitChar(digits[i])));
        sf::FloatRect b = slots[i].getLocalBounds();
        slots[i].setOrigin(b.left + b.width / 2.f, 0.f);
        slots[i].setPosition(slotCenterX(i), kSlotsY);
    }

    status.setFont(font);
    status.setCharacterSize(18);
    status.setFillColor(sf::Color(180, 220, 255));
    status.setString("");

    footer.setFont(font);
    footer.setCharacterSize(14);
    footer.setFillColor(sf::Color(200, 200, 200));
    footer.setStyle(sf::Text::Italic);
    // Show both control schemes so the user knows their input method
    // works without having to discover it.
    footer.setString(
        "Keyboard: 0-9 type   Backspace clear   Enter connect   Esc cancel"
        "  |  Gamepad: LEFT/RIGHT cursor   UP/DOWN digit   A connect   B cancel");
    {
        sf::FloatRect b = footer.getLocalBounds();
        // Shrink if the line is too wide for the screen.
        if (b.width > 700.f) {
            footer.setCharacterSize(12);
            b = footer.getLocalBounds();
        }
        footer.setOrigin(b.left + b.width / 2.f, 0.f);
        footer.setPosition(720.f / 2.f, 480.f - 28.f);
    }

    refreshLabels();
}

void JoinScreen::refreshLabels()
{
    for (int i = 0; i < JoinCode::kCodeLen; ++i) {
        slots[i].setString(std::string(1, JoinCode::digitChar(digits[i])));
        sf::FloatRect b = slots[i].getLocalBounds();
        slots[i].setOrigin(b.left + b.width / 2.f, 0.f);
        slots[i].setPosition(slotCenterX(i), kSlotsY);
        // Active slot pops in yellow; others stay white.
        slots[i].setFillColor(i == cursor ? sf::Color(255, 220, 80)
                                          : sf::Color::White);
    }
    {
        std::ostringstream oss;
        const int code = JoinCode::packDigits(digits);
        if (done) {
            oss << "Connected -- starting...";
        }
        else if (busy) {
            oss << "Searching for host...";
        }
        else if (failed) {
            oss << "No host found on this LAN for that code.  Try again.";
        }
        else {
            // Echo the entered code so the player can confirm what
            // they're about to send.
            oss << "Code: " << JoinCode::encode(code);
        }
        status.setString(oss.str());
        sf::FloatRect b = status.getLocalBounds();
        status.setOrigin(b.left + b.width / 2.f, 0.f);
        status.setPosition(720.f / 2.f, 290.f);
    }
}

void JoinScreen::onUpdate(float /*dt*/)
{
    if (!ready) ready = true;
}

void JoinScreen::tryConnect()
{
    // Strict validation against the same decode() the unit tests use.
    int code = 0;
    {
        std::string s;
        s.reserve(JoinCode::kCodeLen);
        for (int d : digits) s.push_back(JoinCode::digitChar(d));
        if (!JoinCode::decode(s, &code)) {
            failed = true;
            refreshLabels();
            return;
        }
    }

    busy   = true;
    failed = false;
    refreshLabels();

    auto& net = NetworkManager::instance();
    net.disconnect();
    net.setMode(NetworkManager::Mode::CLIENT);
    // Step 1: UDP discovery to resolve the host IP from the room code.
    // 4-digit codes don't encode an address anymore -- the only way the
    // joiner finds the host is to ask on the LAN.
    sf::IpAddress hostIp = sf::IpAddress::None;
    if (!net.clientDiscoverHost(code, sf::seconds(5.f), &hostIp)) {
        net.disconnect();
        net.setMode(NetworkManager::Mode::OFFLINE);
        busy   = false;
        failed = true;
        refreshLabels();
        return;
    }
    // Step 2: open the TCP gameplay socket against the discovered host.
    // clientDiscoverHost already wrote the host's advertised TCP port
    // into NetworkManager's hostPort_, so we just pass the IP here.
    net.setHost(hostIp, 53353);
    const bool ok = net.clientConnectWithTimeout(sf::seconds(5.f));

    busy = false;
    if (!ok) {
        // Fall back to OFFLINE so future inputs aren't intercepted.
        net.disconnect();
        net.setMode(NetworkManager::Mode::OFFLINE);
        failed = true;
        refreshLabels();
        return;
    }
    done = true;
    refreshLabels();
    launchGameplay();
}

void JoinScreen::launchGameplay()
{
    auto& net = NetworkManager::instance();
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

void JoinScreen::cancelToTitle()
{
    auto& net = NetworkManager::instance();
    net.disconnect();
    net.setMode(NetworkManager::Mode::OFFLINE);
    auto evt = std::make_shared< Event<std::string> >("Title");
    Events::triggerEvent("change_screen", evt);
}

void JoinScreen::onGamepadEvent(GamepadEvent e)
{
    if (!ready) return;
    if (done || busy) return;
    if (e.type != GamepadEvent::RELEASED) return;

    if (e.button == "B" || e.button == "MENU") {
        cancelToTitle();
        return;
    }
    if (e.button == "LEFT") {
        cursor = (cursor + JoinCode::kCodeLen - 1) % JoinCode::kCodeLen;
        refreshLabels();
        return;
    }
    if (e.button == "RIGHT") {
        cursor = (cursor + 1) % JoinCode::kCodeLen;
        refreshLabels();
        return;
    }
    if (e.button == "UP") {
        digits[cursor] = (digits[cursor] + 1) % 10;
        refreshLabels();
        return;
    }
    if (e.button == "DOWN") {
        digits[cursor] = (digits[cursor] + 9) % 10; // -1 mod 10
        refreshLabels();
        return;
    }
    if (e.button == "A" || e.button == "START") {
        tryConnect();
        return;
    }
}

void JoinScreen::onKeyInput(const std::string& key)
{
    if (!ready) return;
    if (done || busy) return;
    if (key.empty()) return;

    // Single-digit decimal: write at cursor, auto-advance to make rapid
    // typing feel like a normal text field.
    if (key.size() == 1 && JoinCode::isDecimalDigit(key[0])) {
        digits[cursor] = key[0] - '0';
        if (cursor + 1 < JoinCode::kCodeLen) {
            ++cursor;
        }
        // Failure state is cleared the moment the user starts retyping
        // so the "could not reach host" banner doesn't linger.
        failed = false;
        refreshLabels();
        return;
    }
    if (key == "BACK") {
        // Clear the previous slot and rewind the cursor. If we're
        // already at slot 0, clear in-place.
        if (cursor > 0) --cursor;
        digits[cursor] = 0;
        failed = false;
        refreshLabels();
        return;
    }
    if (key == "ENTER") {
        tryConnect();
        return;
    }
    if (key == "ESC") {
        cancelToTitle();
        return;
    }
}

void JoinScreen::onDraw(sf::RenderTarget& ctx, sf::RenderStates /*states*/) const
{
    ctx.draw(background);
    ctx.draw(title);
    ctx.draw(prompt);
    for (const auto& s : slots) ctx.draw(s);
    ctx.draw(status);
    ctx.draw(footer);
}
