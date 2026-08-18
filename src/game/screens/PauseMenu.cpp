#include "game/screens/PauseMenu.hpp"
#include "engine/Paths.hpp"
#include "engine/AudioPlayer.hpp"
#include <algorithm>

bool PauseMenu::s_inputBlocked = false;

namespace {

constexpr float kLogicalW    = 720.f;
constexpr float kLogicalH    = 480.f;
constexpr float kPanelW      = 480.f;
constexpr float kPanelH      = 320.f;
constexpr float kPanelX      = (kLogicalW - kPanelW) / 2.f;
constexpr float kPanelY      = (kLogicalH - kPanelH) / 2.f;
constexpr float kLineSpacing = 28.f;
constexpr float kListTopY    = kPanelY + 76.f;
constexpr unsigned kTitleSize   = 28;
constexpr unsigned kLineSize    = 18;
constexpr unsigned kFooterSize  = 14;

const sf::Color kBackdropColor  = sf::Color(0, 0, 0, 170);
const sf::Color kPanelFill      = sf::Color(20, 12, 30, 235);
const sf::Color kPanelOutline   = sf::Color(255, 200, 80);
const sf::Color kTitleColor     = sf::Color(255, 220, 120);
const sf::Color kIdleLineColor  = sf::Color(220, 220, 220);
const sf::Color kHotLineColor   = sf::Color(255, 240, 160);
const sf::Color kFooterColor    = sf::Color(180, 180, 180);
const sf::Color kSectionColor   = sf::Color(255, 200, 80);

// Centered helper: positions a text horizontally inside the panel.
void centerInPanel(sf::Text& t, float yAbsolute)
{
    sf::FloatRect b = t.getLocalBounds();
    t.setOrigin(b.left + b.width / 2.f, 0.f);
    t.setPosition(kLogicalW / 2.f, yAbsolute);
}

sf::Text makeText(const std::string& s, unsigned size, sf::Color color,
                  sf::Uint32 style = sf::Text::Regular)
{
    sf::Text t;
    t.setFont(*ResourceManager::getFont(
        Paths::resource("fonts/Underdog-Regular.ttf")));
    t.setString(s);
    t.setCharacterSize(size);
    t.setFillColor(color);
    t.setStyle(style);
    return t;
}

// Items shown on the MAIN page, in display order. Kept in sync with
// PauseMenu::confirmMain (index-based dispatch).
const std::vector<std::string>& mainItems()
{
    static const std::vector<std::string> v = {
        "Resume",
        "Controls",
        "Settings",
        "Exit to Main Menu",
        "Quit Game",
    };
    return v;
}

} // namespace

void PauseMenu::init(GameEngine* eng)
{
    engine = eng;
    state = State::CLOSED;
    mainCursor = 0;
    settingsCursor = 0;

    backdrop.setSize(sf::Vector2f(kLogicalW, kLogicalH));
    backdrop.setFillColor(kBackdropColor);

    panel.setSize(sf::Vector2f(kPanelW, kPanelH));
    panel.setPosition(kPanelX, kPanelY);
    panel.setFillColor(kPanelFill);
    panel.setOutlineColor(kPanelOutline);
    panel.setOutlineThickness(2.f);

    title  = makeText("PAUSED", kTitleSize, kTitleColor, sf::Text::Bold);
    footer = makeText("", kFooterSize, kFooterColor, sf::Text::Italic);

    // Pick up whatever the global volume currently is (defaults to 100
    // in SFML; persists across loads of this object). hh::AudioPlayer
    // keeps the same 0..100 semantics so the slider math is unchanged.
    masterVolume = hh::AudioPlayer::getMasterVolume();
    masterVolume = std::max(0.f, std::min(100.f, masterVolume));

    ready = true;
}

void PauseMenu::open()
{
    if (!ready) {
        return;
    }
    s_inputBlocked = true;
    enterState(State::MAIN);
}

void PauseMenu::close()
{
    state = State::CLOSED;
    s_inputBlocked = false;
}

bool PauseMenu::handleEvent(const GamepadEvent& e)
{
    if (!isOpen()) {
        return false;
    }
    if (e.type != GamepadEvent::RELEASED) {
        // Still consumes the press: we don't want the press to register
        // with gameplay while the menu sits on top, only releases drive
        // navigation (matches GametitleScreen / ControlsScreen).
        return true;
    }

    // MENU button toggles the overlay closed from any sub-page.
    if (e.button == "MENU") {
        close();
        return true;
    }

    if (state == State::MAIN) {
        if (e.button == "UP") {
            mainCursor = (mainCursor + static_cast<int>(mainItems().size()) - 1)
                       % static_cast<int>(mainItems().size());
            refreshSelection();
        }
        else if (e.button == "DOWN") {
            mainCursor = (mainCursor + 1)
                       % static_cast<int>(mainItems().size());
            refreshSelection();
        }
        else if (e.button == "A" || e.button == "START") {
            confirmMain();
        }
        else if (e.button == "B") {
            // B at the top level is a no-op (use MENU to close) -- prevents
            // accidentally resuming when the player meant to back out of a
            // sub-page they had already left.
        }
        return true;
    }

    if (state == State::CONTROLS) {
        // Any release returns to the main page (matches ControlsScreen UX).
        if (e.button == "A" || e.button == "B" || e.button == "START") {
            enterState(State::MAIN);
        }
        return true;
    }

    if (state == State::SETTINGS) {
        const int kNumSettings = 2; // volume, fullscreen
        if (e.button == "UP") {
            settingsCursor = (settingsCursor + kNumSettings - 1) % kNumSettings;
            refreshSelection();
        }
        else if (e.button == "DOWN") {
            settingsCursor = (settingsCursor + 1) % kNumSettings;
            refreshSelection();
        }
        else if (e.button == "LEFT") {
            changeSetting(-1);
        }
        else if (e.button == "RIGHT" || e.button == "A" || e.button == "START") {
            changeSetting(+1);
        }
        else if (e.button == "B") {
            enterState(State::MAIN);
        }
        return true;
    }

    return true;
}

void PauseMenu::enterState(State s)
{
    state = s;
    switch (s) {
        case State::MAIN:     buildMainPage();     break;
        case State::CONTROLS: buildControlsPage(); break;
        case State::SETTINGS: buildSettingsPage(); break;
        case State::CLOSED:                        break;
    }
}

void PauseMenu::confirmMain()
{
    switch (mainCursor) {
        case 0: // Resume
            close();
            return;
        case 1: // Controls
            enterState(State::CONTROLS);
            return;
        case 2: // Settings
            enterState(State::SETTINGS);
            return;
        case 3: // Exit to Main Menu
        {
            close();
            auto ev = std::make_shared< Event<std::string> >("Title");
            Events::triggerEvent("change_screen", ev);
            return;
        }
        case 4: // Quit Game
            if (engine) {
                engine->exit();
            }
            return;
        default:
            return;
    }
}

void PauseMenu::changeSetting(int delta)
{
    if (settingsCursor == 0) {
        // Master volume: 10-unit steps so a few presses span the range.
        setMasterVolume(masterVolume + delta * 10.f);
        refreshSettingsLabels();
        refreshSelection();
    }
    else if (settingsCursor == 1) {
        // Fullscreen: any direction toggles (boolean setting).
        if (engine) {
            engine->toggleFullscreen();
        }
        refreshSettingsLabels();
        refreshSelection();
    }
}

void PauseMenu::setMasterVolume(float v)
{
    masterVolume = std::max(0.f, std::min(100.f, v));
    hh::AudioPlayer::setMasterVolume(masterVolume);
}

void PauseMenu::buildMainPage()
{
    title.setString("PAUSED");
    centerInPanel(title, kPanelY + 24.f);

    lines.clear();
    const auto& items = mainItems();
    lines.reserve(items.size());
    float y = kListTopY;
    for (const auto& s : items) {
        sf::Text t = makeText(s, kLineSize, kIdleLineColor, sf::Text::Bold);
        centerInPanel(t, y);
        lines.push_back(std::move(t));
        y += kLineSpacing;
    }

    footer.setString("UP/DOWN select | A/Z confirm | press menu key to resume");
    centerInPanel(footer, kPanelY + kPanelH - 28.f);
    refreshSelection();
}

void PauseMenu::buildControlsPage()
{
    title.setString("CONTROLS");
    centerInPanel(title, kPanelY + 24.f);

    struct Row { std::string text; bool header; };
    // Toggle BRO-only lines so a non-BRO party doesn't see a sprint
    // binding they can't actually use. When no one is BRO, fold the
    // menu binding back onto its own line so the row still reads
    // cleanly.
    std::vector<Row> rows = {
        { "KEYBOARD",                          true  },
        { "Move .......... Arrow Keys",        false },
        { "Attack ........ X    Read clue . Z", false },
        { "Cycle weapon .. V",                 false },
    };
    if (broActive) {
        rows.push_back({ "BRO sprint .... C    Menu ...... Esc", false });
    }
    else {
        rows.push_back({ "Menu .......... Esc",                false });
    }
    rows.push_back({ "",                                false });
    rows.push_back({ "GAMEPAD",                         true  });
    rows.push_back({ "Move .......... D-Pad / Stick",   false });
    rows.push_back({ "Attack ........ B    Read clue . A", false });
    rows.push_back({ "Cycle weapon .. Y",                  false });
    if (broActive) {
        rows.push_back({ "BRO sprint .... X    Menu ...... Start", false });
    }
    else {
        rows.push_back({ "Menu .......... Start",              false });
    }

    lines.clear();
    lines.reserve(rows.size());
    float y = kListTopY - 12.f;
    for (const auto& row : rows) {
        if (row.text.empty()) {
            y += kLineSpacing / 2.f;
            continue;
        }
        const sf::Color color = row.header ? kSectionColor : kIdleLineColor;
        sf::Text t = makeText(row.text, kLineSize, color,
                              row.header ? sf::Text::Bold : sf::Text::Regular);
        centerInPanel(t, y);
        lines.push_back(std::move(t));
        y += row.header ? (kLineSpacing - 2.f) : (kLineSpacing - 4.f);
    }

    footer.setString("Spectator: LEFT/RIGHT watch | A warn room | B back");
    centerInPanel(footer, kPanelY + kPanelH - 28.f);
}

void PauseMenu::buildSettingsPage()
{
    title.setString("SETTINGS");
    centerInPanel(title, kPanelY + 24.f);
    settingsCursor = std::max(0, std::min(1, settingsCursor));
    lines.clear();
    lines.reserve(2);
    // Empty placeholders -- refreshSettingsLabels fills them in based on
    // the current volume / fullscreen state.
    lines.push_back(makeText("", kLineSize, kIdleLineColor, sf::Text::Bold));
    lines.push_back(makeText("", kLineSize, kIdleLineColor, sf::Text::Bold));

    footer.setString("UP/DOWN select | LEFT/RIGHT change | B back");
    centerInPanel(footer, kPanelY + kPanelH - 28.f);
    refreshSettingsLabels();
    refreshSelection();
}

void PauseMenu::refreshSettingsLabels()
{
    if (lines.size() < 2) {
        return;
    }
    // Volume: render as "Master Volume:  [#####.....]  50%" so the bar
    // gives an at-a-glance feel without needing a separate slider widget.
    const int filled = static_cast<int>(masterVolume / 10.f + 0.5f);
    std::string bar = "[";
    for (int i = 0; i < 10; ++i) {
        bar += (i < filled) ? '#' : '.';
    }
    bar += "]";
    std::string volLine = "Master Volume:  " + bar
        + "  " + std::to_string(static_cast<int>(masterVolume + 0.5f)) + "%";
    lines[0].setString(volLine);
    centerInPanel(lines[0], kListTopY + 20.f);

    const bool fs = engine && engine->isFullscreen();
    std::string fsLine = std::string("Fullscreen:     ")
                       + (fs ? "ON" : "OFF") + "   (LEFT/RIGHT toggles)";
    lines[1].setString(fsLine);
    centerInPanel(lines[1], kListTopY + 20.f + kLineSpacing + 8.f);
}

void PauseMenu::refreshSelection()
{
    int cursor = (state == State::MAIN)     ? mainCursor
               : (state == State::SETTINGS) ? settingsCursor
               : -1;
    for (size_t i = 0; i < lines.size(); ++i) {
        const bool hot = (static_cast<int>(i) == cursor);
        // Don't recolor section headers on the controls page: there's no
        // cursor there, so refreshSelection won't be called for it.
        lines[i].setFillColor(hot ? kHotLineColor : kIdleLineColor);
        lines[i].setStyle(hot ? (sf::Text::Bold | sf::Text::Underlined)
                              : sf::Text::Bold);
    }
}

void PauseMenu::draw(sf::RenderTarget& target) const
{
    if (state == State::CLOSED) {
        return;
    }
    // Reset to the logical 720x480 default view so the menu is laid out
    // in canvas pixels regardless of any per-player view the gameplay
    // code left active when it called this.
    const sf::View saved = target.getView();
    target.setView(target.getDefaultView());
    target.draw(backdrop);
    target.draw(panel);
    target.draw(title);
    for (const auto& l : lines) {
        target.draw(l);
    }
    target.draw(footer);
    target.setView(saved);
}
