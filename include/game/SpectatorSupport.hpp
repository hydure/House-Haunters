#ifndef SPECTATOR_SUPPORT_HPP
#define SPECTATOR_SUPPORT_HPP

class Room;

class SpectatorSupport
{
public:
    static SpectatorSupport& instance();

    void reset();
    void update(float dt);
    bool ping(Room* room, int spectatorPlayer);

    bool activeIn(const Room* room) const;
    int signaledBy() const { return signaledBy_; }
    float cooldownRemaining() const { return cooldownRemainingSec_; }

private:
    Room* room_ = nullptr;
    int signaledBy_ = -1;
    float activeRemainingSec_ = 0.f;
    float cooldownRemainingSec_ = 0.f;
};

#endif