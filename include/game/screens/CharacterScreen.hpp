#ifndef CHARACTER_SCREEN_HPP
#define CHARACTER_SCREEN_HPP

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <SFML/Graphics.hpp>
#include "engine/Engine.hpp"
#include "engine/AudioPlayer.hpp"

class CharacterIcon : public GameObject
{
public:
    void init() override {}
    void setColor(sf::Color color) { t.setFillColor(color); }
    void setFont(std::string font) { t.setFont(*ResourceManager::getFont(font)); }
    void setPlayer(int num) { player_number = num; t.setString("P" + std::to_string(num)); }
    int  getPlayer() const { return player_number; }
    void onDraw(sf::RenderTarget& ctx, sf::RenderStates states) const override;

protected:
    int player_number = -1;
    sf::Text t;
};


class CharacterSelection : public GameObject
{
public:
    void init() override {}
    void setPortrait(int index);
    void setPlayer(int player);
    void unsetPlayer();
    int  getPlayer() const { return player_selected; }
    bool isSelected() const { return selected; }
    void removePlayer(int num);
    void addPlayer(int num);
    bool hasPlayer(int num);
    bool isEmpty() const { return hovering.empty(); }
    void onDraw(sf::RenderTarget& ctx, sf::RenderStates states) const override;
    void onUpdate(float dt) override;
    void setCharacter(Config::CHARACTER c) { character = c; }
    Config::CHARACTER getCharacter() const { return character; }

protected:
    Config::CHARACTER character = Config::CHARACTER::MOM;
    int index = 0;
    sf::Color colors[4] = { sf::Color::Red, sf::Color(30, 144, 255), sf::Color::Green, sf::Color::Yellow };
    std::vector<std::unique_ptr<CharacterIcon>>::iterator find(int player);
    bool selected = false;
    int player_selected = -1;
    sf::RectangleShape background;
    sf::Sprite portrait;
    std::vector<std::unique_ptr<CharacterIcon>> hovering;
};


class CharacterScreen : public GameScreen
{
public:
    void init() override;
    void onDraw(sf::RenderTarget& ctx, sf::RenderStates states) const override;
    void onUpdate(float dt) override;

    // Test hook: returns true when the screen is in "press any button to
    // start the lobby" mode (no P1 yet, waiting for the first input).
    bool isAwaitingFirstPlayer() const { return awaiting_p1; }
    // Test hook: returns the number of players currently in the lobby
    // (some hovering, some locked in).
    int  playerCount() const { return player_num; }

protected:
    void onGamepadEvent(GamepadEvent e);
    void onGamepadConnect(GamepadEvent e);
    void addPlayer(int num, int index);

    std::vector<std::unique_ptr<CharacterSelection>> char_selections;
    bool changed = false;
    sf::RectangleShape background;
    sf::Text teamFont;
    sf::RectangleShape blackness;
    int player_num = 0;
    float trans = 255.f;
    int selected_count = 0;
    hh::Sound chara_sound;

    // True while the screen is showing the "PRESS ANY BUTTON / PLUG IN
    // EXTRA CONTROLLERS" gate, before the first player has joined. The
    // first PRESSED gamepad event (any button on any controller) drops
    // the gate and registers that input source as P1. While the gate
    // is up, hot-plugged controllers are silently registered as
    // P2/P3/P4 so they're ready to start picking the moment the host
    // hits a button.
    bool awaiting_p1 = false;
};

#endif
