// Hitbox inherits from sf::FloatRect for cheap AABB queries. We don't need
// to draw anything here -- just confirm the geometry behaves like a rect.

#include "test_harness.hpp"
#include "components/Hitbox.hpp"

TEST_CASE("Hitbox default-constructs to a zero-size rect")
{
    Hitbox h;
    CHECK_EQ(h.left, 0.f);
    CHECK_EQ(h.top, 0.f);
    CHECK_EQ(h.width, 0.f);
    CHECK_EQ(h.height, 0.f);
}

TEST_CASE("Hitbox(x,y,w,h) stores the rect verbatim")
{
    Hitbox h(10.f, 20.f, 30.f, 40.f);
    CHECK_EQ(h.left, 10.f);
    CHECK_EQ(h.top, 20.f);
    CHECK_EQ(h.width, 30.f);
    CHECK_EQ(h.height, 40.f);
}

TEST_CASE("Hitbox intersects overlapping rect")
{
    Hitbox a(0.f, 0.f, 10.f, 10.f);
    sf::FloatRect b(5.f, 5.f, 10.f, 10.f);
    CHECK(a.intersects(b));
}

TEST_CASE("Hitbox does not intersect disjoint rect")
{
    Hitbox a(0.f, 0.f, 10.f, 10.f);
    sf::FloatRect b(20.f, 20.f, 5.f, 5.f);
    CHECK(!a.intersects(b));
}

TEST_CASE("Hitbox::contains: point inside vs outside")
{
    Hitbox a(0.f, 0.f, 10.f, 10.f);
    CHECK(a.contains(5.f, 5.f));
    CHECK(!a.contains(15.f, 15.f));
}

TEST_CASE("Hitbox: edge-touching counts as intersecting (SFML semantics)")
{
    Hitbox a(0.f, 0.f, 10.f, 10.f);
    sf::FloatRect right(10.f, 0.f, 1.f, 10.f); // shares the x=10 edge
    // SFML treats edge-only contact as non-intersecting (FloatRect uses
    // strict less-than for the "max" side), which is what the gameplay
    // collision code assumes. If you ever flip this, lots of code breaks.
    CHECK(!a.intersects(right));
}

// A trivial GameObject we can hand to Hitbox::follow without dragging
// any subclass behavior in. setPosition() comes from sf::Transformable.
class TrackerStub : public GameObject {};

TEST_CASE("Hitbox::follow: tracks parent position, preserves initial offset")
{
    TrackerStub parent;
    parent.setPosition(100.f, 200.f);

    Hitbox h(5.f, 7.f, 32.f, 32.f); // offset of (5, 7) gets baked in by init()
    h.init();
    h.follow(&parent);

    // First update applies the stored offset to the tracker's current position.
    h.onUpdate(0.016f);
    CHECK_EQ(h.left, 105.f);
    CHECK_EQ(h.top, 207.f);

    parent.setPosition(50.f, 60.f);
    h.onUpdate(0.016f);
    CHECK_EQ(h.left, 55.f);
    CHECK_EQ(h.top, 67.f);
}

TEST_CASE("Hitbox::onUpdate: unbound hitbox stays put")
{
    Hitbox h(11.f, 22.f, 4.f, 4.f);
    h.init();
    // No follow() call. update() must not touch left/top.
    h.onUpdate(0.016f);
    CHECK_EQ(h.left, 11.f);
    CHECK_EQ(h.top, 22.f);
}

TEST_CASE("Hitbox::follow: two hitboxes on the same tracker get independent offsets")
{
    TrackerStub parent;
    parent.setPosition(0.f, 0.f);

    Hitbox feet(0.f, 30.f, 16.f, 4.f);
    feet.init();
    feet.follow(&parent);

    Hitbox head(0.f, 0.f, 16.f, 8.f);
    head.init();
    head.follow(&parent);

    parent.setPosition(100.f, 100.f);
    feet.onUpdate(0.016f);
    head.onUpdate(0.016f);

    CHECK_EQ(feet.top, 130.f);
    CHECK_EQ(head.top, 100.f);
    CHECK_EQ(feet.left, 100.f);
    CHECK_EQ(head.left, 100.f);
}

TEST_MAIN()
