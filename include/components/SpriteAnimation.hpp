/////////////////
// SpriteAnimation.hpp
//
// A simple frame-based sprite animation. Frames are described as tile
// indices into a sprite-sheet texture; advancing the animation cycles
// through the configured frames and updates the underlying sf::Sprite.
//
/////////////////

#ifndef SPRITE_ANIMATION_HPP
#define SPRITE_ANIMATION_HPP

#include <functional>
#include <vector>
#include <SFML/Graphics.hpp>
#include "engine/Engine.hpp"

class SpriteAnimation : public GameObject
{
public:
    SpriteAnimation() = default;

    void setSpriteSheet(sf::Texture& t);
    void addFrames(const std::vector<std::vector<int>>& frames, int tileW, int tileH);
    void addFrame(const std::vector<int>& frame, int tileW, int tileH);

    void onDraw(sf::RenderTarget& target, sf::RenderStates states) const override;

    // Advance to the next frame if enough simulated time has accumulated.
    void nextFrame(float dt);

    void play() { playing = true; }
    void play(std::function<void()> cb) {
        playing = true;
        onComplete = std::move(cb);
        performAfterPlayer = true;
    }
    void stop() { playing = false; }
    bool isPlaying() const { return playing; }

protected:
    std::function<void()> onComplete;
    float time = 0;
    int curr_frame = 0;
    bool playing = true;
    bool performAfterPlayer = false;
    sf::Sprite sprite;
    sf::Texture* texture = nullptr;
    std::vector<sf::IntRect> m_frames;
};

#endif