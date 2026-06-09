#ifndef GAME_OBJECT_HPP
#define GAME_OBJECT_HPP

#include <SFML/Graphics.hpp>
#include <memory>
#include <list>

////////////////////////////////
// Base scene-graph node. Owns child GameObjects and forwards
// update/draw down the tree.
//
// Ownership rules (read before adding new nodes):
//   * Subclasses MAY use `children` (via addChild) for *nested
//     composition* -- update/draw cascade automatically.
//   * Top-level game state does NOT live in the scene graph. Characters
//     and Clues are owned by EntityGroup's own vectors; Rooms are owned
//     by RoomGroup's own vector. Those containers manage iteration
//     order, lookup-by-id, and spatial culling -- responsibilities that
//     a generic `children` list can't express. See the comments at the
//     top of EntityGroup.hpp / RoomGroup.hpp for details.
//   * As a rule of thumb: if you need ordered draw, fast lookup, or
//     spatial queries over a collection, store it in a typed vector on
//     the owning component. Use addChild only for parent/child transform
//     and lifetime piggy-backing.
////////////////////////////////
class GameObject: public sf::Transformable, public sf::Drawable
{
public:
    int z_index = 0;
    typedef std::unique_ptr<GameObject> GameObjectPtr;

    GameObject() = default;
    GameObject(sf::Vector2f p) : relPos(p) {};
    GameObject(int t, int l) : relPos(static_cast<float>(t), static_cast<float>(l)) {};
    // Polymorphic base: needed because children are owned through
    // unique_ptr<GameObject> and erased through the base type.
    virtual ~GameObject() = default;

    // Non-copyable: `children` holds unique_ptr<GameObject>, so the implicit
    // copy ops can't be synthesized. MSVC's /permissive- mode will eagerly
    // try to instantiate them in derived classes (C2280) unless we delete
    // them here. Default the moves since declaring the copy ops suppresses
    // the implicit move generation.
    GameObject(const GameObject&)            = delete;
    GameObject& operator=(const GameObject&) = delete;
    GameObject(GameObject&&)                 = default;
    GameObject& operator=(GameObject&&)      = default;

    // Override these in subclasses
    virtual void init(){};
    virtual void onUpdate(float /*dt*/){};
    virtual void onDraw(sf::RenderTarget& /*ctx*/, sf::RenderStates /*states*/) const{};

    void addChild(GameObjectPtr o);
    void update(float dt);

    std::list<GameObjectPtr> children;

protected:
    // Position relative to parent
    sf::Vector2f relPos{0, 0};
    // Parent GameObject
    GameObject* m_parent = nullptr;

    void setParent(GameObject* p){ this->m_parent = p; };
    // inherited from sf::Drawable
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
};


#endif
