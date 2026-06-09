#ifndef PLAYERVIEW_HPP
#define PLAYERVIEW_HPP

#include <memory>
#include <SFML/Graphics.hpp>
#include "engine/Engine.hpp"
#include "game/characters/Character.hpp"
#include "game/characters/Villain.hpp"
#include "game/rooms/RoomGroup.hpp"
#include "components/EntityGroup.hpp"
#include "game/objects/Clue.hpp"

////////////////
// PlayerView.hpp
//
// View used to follow a particular player around inside the game.  Up to
// four PlayerViews can exist simultaneously (one per player). Each one
// owns its sf::View and HUD.
////////////////
class PlayerView : public GameObject
{
public:
    PlayerView() = default;
    void init() override;
    void onUpdate(float dt) override;

    void setEntities(EntityGroup* ents) { entity_group = ents; }
    void setView(sf::FloatRect dimensions, sf::FloatRect viewport);
    void setEntityNumber(int number) { playernumber = number; }
    void setControllerIndex(int /*index*/) {}
    void setRoomGroup(RoomGroup* g) { this->rooms = g; }

    // Spectator mode: when this view's owning character has died, the
    // camera / HUD switch to following another living player so the dead
    // player can still watch the action and cycle between survivors with
    // LEFT / RIGHT.
    //
    //   * currentTarget() returns the player_number the view should be
    //     centered on right now -- own player while alive, or the
    //     spectator target while dead. -1 means "no valid target"
    //     (everyone is dead -- the game-end transition is about to fire
    //     anyway).
    //   * spectatorTarget() exposes the underlying override (-1 when
    //     not in spectator mode) for tests.
    //   * cycleSpectator(+1 / -1) moves to the next / previous living
    //     teammate; no-op while own character is still alive.
    //   * ensureSpectator() validates / picks the spectator target;
    //     called every frame and by the gamepad listener.
    int  currentTarget() const;
    int  spectatorTarget() const { return spectatorTarget_; }
    int  ownPlayerNumber() const { return playernumber; }
    void cycleSpectator(int delta);
    void ensureSpectator();

    sf::FloatRect viewDimensions;
    // Number of active players; used by the lighting shader.
    int numPlayers = -1;

protected:
    void onDraw(sf::RenderTarget& target, sf::RenderStates states) const override;

    float viewport_x = 0.f;
    float viewport_y = 0.f;
    sf::Shader shader;
    // True only when the GLSL lighting shader successfully loaded *and*
    // the host OpenGL context advertises shader support. When false, the
    // HUD lighting is drawn with no shader (vignette disabled) instead
    // of binding an empty Shader -- the latter floods stderr with
    // "Failed to bind or unbind shader" once per frame per player on
    // machines stuck on Microsoft's GDI Generic software renderer.
    bool shaderReady = false;
    sf::RectangleShape lighting;
    RoomGroup* rooms = nullptr;
    int playernumber = 0;
    EntityGroup* entity_group = nullptr;
    // -1 means "not in spectator mode -- follow our own playernumber".
    // Anything > 0 is the player_number of the teammate we're watching
    // because our own character died.
    int spectatorTarget_ = -1;
    sf::View v;
    sf::View HUD;
    sf::RectangleShape itemBar;
    sf::Texture heartTexture;
    sf::RectangleShape pain;
    sf::Clock clock;
    int painCount = 0;

    // Cached HUD sprites/shapes. Built once (init / setView) and mutated
    // in-place from the const onDraw to avoid the per-frame allocations
    // that used to happen for hearts + clue overlay.
    mutable sf::Sprite heartSprite;
    mutable sf::RectangleShape clueBgBox;
    mutable sf::Text clueText;
    mutable bool clueTextReady = false;

    // RAII handle for the gamepad listener registered in init(); removed
    // automatically when the PlayerView is destroyed.
    EventSubscription gamepadSub;
};

#endif