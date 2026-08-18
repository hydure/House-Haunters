#include "game/SpectatorSupport.hpp"

#include <algorithm>

SpectatorSupport& SpectatorSupport::instance()
{
    static SpectatorSupport support;
    return support;
}

void SpectatorSupport::reset()
{
    room_ = nullptr;
    signaledBy_ = -1;
    activeRemainingSec_ = 0.f;
    cooldownRemainingSec_ = 0.f;
}

void SpectatorSupport::update(float dt)
{
    if (dt <= 0.f) return;
    activeRemainingSec_ = std::max(0.f, activeRemainingSec_ - dt);
    cooldownRemainingSec_ = std::max(0.f, cooldownRemainingSec_ - dt);
    if (activeRemainingSec_ <= 0.f) {
        room_ = nullptr;
        signaledBy_ = -1;
    }
}

bool SpectatorSupport::ping(Room* room, int spectatorPlayer)
{
    if (room == nullptr || cooldownRemainingSec_ > 0.f) return false;
    room_ = room;
    signaledBy_ = spectatorPlayer;
    activeRemainingSec_ = 4.f;
    cooldownRemainingSec_ = 12.f;
    return true;
}

bool SpectatorSupport::activeIn(const Room* room) const
{
    return room != nullptr && room_ == room && activeRemainingSec_ > 0.f;
}