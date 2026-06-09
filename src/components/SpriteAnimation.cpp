#include "components/SpriteAnimation.hpp"

void SpriteAnimation::setSpriteSheet(sf::Texture& t)
{
    texture = &t;
    sprite.setTexture(t);
}

void SpriteAnimation::addFrames(const std::vector<std::vector<int>>& frames, int tileW, int tileH)
{
    for (const auto& frame : frames) {
        addFrame(frame, tileW, tileH);
    }
}

void SpriteAnimation::addFrame(const std::vector<int>& frame, int tileW, int tileH)
{
    // TODO: Factor in sprite width and sprite height in order to account
    //       for larger sprite compositions.
    const sf::Vector2u size = texture->getSize();
    const int tilesPerRow = size.x / tileW;

    const bool wasEmpty = m_frames.empty();
    for (int tileNum : frame) {
        const int tx = tileNum % tilesPerRow;
        const int ty = tileNum / tilesPerRow;
        m_frames.emplace_back(tx * tileW, ty * tileH, tileW, tileH);
    }
    // Prime the sprite with the very first frame that was added.
    if (wasEmpty && !m_frames.empty()) {
        sprite.setTextureRect(m_frames.front());
    }
}

void SpriteAnimation::nextFrame(float dt)
{
    if (!playing) {
        return;
    }
    time += dt;
    if (time < 6.0f * dt) {
        return;
    }

    sprite.setTextureRect(m_frames.at(curr_frame));

    if (performAfterPlayer && (curr_frame + 1 == static_cast<int>(m_frames.size()))) {
        onComplete();
        stop();
        curr_frame = 0;
    } else {
        curr_frame = (curr_frame + 1) % m_frames.size();
    }
    time = 0;
}

void SpriteAnimation::onDraw(sf::RenderTarget& target, sf::RenderStates states) const
{
    target.draw(sprite, states);
}