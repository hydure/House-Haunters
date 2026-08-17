#include "game/screens/ControlsScreen.hpp"
#include "engine/Paths.hpp"

namespace {

// Style helpers shared by every line on the screen.
constexpr unsigned kHeaderSize  = 22;
constexpr unsigned kBodySize    = 16;
constexpr float    kLineSpacing = 22.f;
constexpr float    kLeftMargin  = 40.f;
constexpr float    kFirstLineY  = 80.f;

sf::Text makeText(const sf::Font& font,
                  const std::string& s,
                  unsigned size,
                  sf::Color color,
                  sf::Uint32 style = sf::Text::Regular)
{
    sf::Text t;
    t.setFont(font);
    t.setString(s);
    t.setCharacterSize(size);
    t.setFillColor(color);
    t.setStyle(style);
    return t;
}

} // namespace

void ControlsScreen::init()
{
    ready = false;

    // RAII subscription -- dropped automatically when the engine leaves us.
    this->subscribe("gamepad_event", [this](base_event_type e) {
        auto gpe = dynamic_cast< GamepadEvent& >(*e);
        this->onGamepadEvent(gpe);
    });

    background.setSize(sf::Vector2f(720.f, 480.f));
    background.setFillColor(sf::Color(10, 5, 20, 235));

    const sf::Font& font = *ResourceManager::getFont(
        Paths::resource("fonts/Underdog-Regular.ttf"));

    title = makeText(font, "CONTROLS", 36, sf::Color::White, sf::Text::Bold);
    {
        sf::FloatRect b = title.getLocalBounds();
        title.setOrigin(b.left + b.width / 2.f, 0.f);
        title.setPosition(720.f / 2.f, 20.f);
    }

    footer = makeText(font,
                      "Press any button to return to the title screen",
                      16, sf::Color(220, 220, 220), sf::Text::Italic);
    {
        sf::FloatRect b = footer.getLocalBounds();
        footer.setOrigin(b.left + b.width / 2.f, 0.f);
        footer.setPosition(720.f / 2.f, 480.f - 30.f);
    }

    // The lines, in display order. Two columns would be nicer but a single
    // vertical list keeps the layout robust to viewport changes.
    struct Line { std::string text; bool header; };
    const std::vector<Line> rows = {
        { "KEYBOARD",                         true  },
        { "Move .......... Arrow Keys",       false },
        { "Attack ........ X",                false },
        { "Read clue ..... Z",                false },
        { "Cycle weapon .. V",                false },
        { "BRO sprint .... C  (hold)",        false },
        { "Open controls . V  (title only)",  false },
        { "",                                  false },
        { "GAMEPAD",                          true  },
        { "Move .......... D-Pad / Left Stick", false },
        { "Attack ........ B",                false },
        { "Read clue ..... A",                false },
        { "Cycle weapon .. Y",                false },
        { "BRO sprint .... X  (hold)",        false },
        { "Open controls . Y  (title only)",  false },
        { "",                                  false },
        { "CHARACTER ABILITIES",              true  },
        { "MOM 'Hunch' ... Guaranteed with a teammate; 50% alone",          false },
        { "DAD 'Protector' Absorbs a nearby ally hit every 10 seconds",      false },
        { "BRO 'Decoy' ... Sprinting draws the ghost away from allies",      false },
        { "SIS 'Stealth' . Invisible to the villain while standing still",  false },
    };

    lines.clear();
    lines.reserve(rows.size());
    float y = kFirstLineY;
    for (const Line& row : rows) {
        if (row.text.empty()) {
            y += kLineSpacing / 2.f; // small gap between sections
            continue;
        }
        const unsigned size = row.header ? kHeaderSize : kBodySize;
        const sf::Color color = row.header
            ? sf::Color(255, 200, 80)   // warm orange for section labels
            : sf::Color::White;
        const sf::Uint32 style = row.header ? sf::Text::Bold : sf::Text::Regular;
        sf::Text t = makeText(font, row.text, size, color, style);
        t.setPosition(kLeftMargin, y);
        lines.push_back(std::move(t));
        y += row.header ? (kLineSpacing + 4.f) : kLineSpacing;
    }
}

void ControlsScreen::onUpdate(float /*dt*/)
{
    // Latch ready=true a single frame after init so the press that opened
    // this screen doesn't immediately close it.
    if (!ready) {
        ready = true;
    }
}

void ControlsScreen::onGamepadEvent(GamepadEvent e)
{
    if (!ready) {
        return;
    }
    // Treat the *release* of any button as "go back", same convention the
    // title and story screens already use.
    if (e.type != GamepadEvent::RELEASED) {
        return;
    }
    auto event = std::make_shared< Event<std::string> >("Title");
    Events::triggerEvent("change_screen", event);
}

void ControlsScreen::onDraw(sf::RenderTarget& ctx, sf::RenderStates /*states*/) const
{
    ctx.draw(background);
    ctx.draw(title);
    for (const auto& l : lines) {
        ctx.draw(l);
    }
    ctx.draw(footer);
}
