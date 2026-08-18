#ifndef GAME_TITLE_SCREEN_HPP
#define GAME_TITLE_SCREEN_HPP

#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>
#include "engine/Engine.hpp"
#include "engine/AudioPlayer.hpp"
#include "engine/ResourceManager.hpp"


class GametitleScreen : public GameScreen
{
public:
    void init() override;
    void onDraw(sf::RenderTarget& ctx, sf::RenderStates states) const override;
    void onUpdate(float dt) override;

    // Returns whether the player has already gone through the
    // "press any button to continue" gate during this session. The
    // flag is intentionally NOT reset by init() so that returning to
    // the title from Host/Join/Character lands the user back on the
    // menu directly (no extra button press needed). Exposed publicly
    // for the screen-flow regression test.
    bool hasBeenPressed() const { return pressed; }

protected:
    void onGamepadEvent(GamepadEvent e);
    // Refreshes difficulty_text from config->difficulty.
    void updateDifficultyText();
    // Refreshes the visible main-menu list (re-colors the highlighted line).
    void updateMenuText();
    // Confirms the menu item currently under the cursor.
    void confirmMenuSelection();
    hh::Music music;
    bool changed = false;
    bool pressed = false;
    sf::Sprite sprite;
    sf::RectangleShape blackness;
    sf::Text difficulty_text;
    sf::Text difficulty_detail;
    // Hint reminding the player they can open the controls screen.
    sf::Text controls_hint;
    // "Press Any Button to Continue / Start" prompt. Its string changes
    // after the first acknowledgement press so the user can tell that
    // the second press is what actually advances to character select.
    sf::Text continue_prompt;
    // Drives a gentle alpha pulse on continue_prompt so it reads as a
    // call-to-action rather than static UI chrome.
    float prompt_time = 0.f;
    float trans = 255.f;

    // Main-menu items (LOCAL PLAY / HOST GAME / JOIN GAME) and which one
    // the cursor is currently on. The menu becomes visible after the
    // first acknowledgement press so it doesn't fight the title art.
    std::vector<sf::Text> menu_lines;
    int menu_cursor = 0;
    // Semi-transparent dark plate drawn behind the menu so the white /
    // yellow text reads against the busy logo. Sized in init() based on
    // the menu position so it always frames the items.
    sf::RectangleShape menu_backdrop;
};

#endif
