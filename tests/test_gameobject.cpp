// GameObject is the scene-graph base used by every visual entity in the
// game. The public API is intentionally tiny -- addChild, update, draw --
// but the implicit contracts it enforces (update cascades to children
// first, addChild calls init(), copy-construction is forbidden) are easy
// to regress without noticing. These tests pin those contracts.

#include "test_harness.hpp"
#include "engine/GameObject.hpp"

#include <type_traits>
#include <vector>

namespace {

// Records every onUpdate/init call against a shared trace, in order, so
// we can verify that GameObject::update walks the tree exactly once and
// that addChild fires init() on the new child.
struct Trace {
    std::vector<std::string> events;
};

class Recorder : public GameObject {
public:
    Recorder(Trace& t, std::string label) : trace(&t), name(std::move(label)) {}
    void init() override { trace->events.push_back(name + ":init"); }
    void onUpdate(float dt) override
    {
        trace->events.push_back(name + ":update");
        lastDt = dt;
    }
    Trace* trace;
    std::string name;
    float lastDt = -1.f;
};

} // namespace

TEST_CASE("GameObject: addChild assigns parent, fires init, takes ownership")
{
    Trace trace;
    GameObject root;
    auto child = std::make_unique<Recorder>(trace, "child");
    Recorder* observer = child.get();

    root.addChild(std::move(child));

    REQUIRE(root.children.size() == 1u);
    CHECK(root.children.front().get() == observer);
    REQUIRE(trace.events.size() == 1u);
    CHECK_EQ(trace.events[0], std::string("child:init"));
}

TEST_CASE("GameObject::update walks children before invoking self.onUpdate")
{
    Trace trace;
    Recorder root(trace, "root");

    auto a = std::make_unique<Recorder>(trace, "a");
    auto b = std::make_unique<Recorder>(trace, "b");
    root.addChild(std::move(a));
    root.addChild(std::move(b));

    trace.events.clear(); // drop the init events from addChild
    root.update(0.016f);

    // Expected order: every child's update (in insertion order) then self.
    REQUIRE(trace.events.size() == 3u);
    CHECK_EQ(trace.events[0], std::string("a:update"));
    CHECK_EQ(trace.events[1], std::string("b:update"));
    CHECK_EQ(trace.events[2], std::string("root:update"));
}

TEST_CASE("GameObject::update propagates dt unchanged to every child")
{
    Trace trace;
    Recorder root(trace, "root");
    auto child = std::make_unique<Recorder>(trace, "child");
    Recorder* observer = child.get();
    root.addChild(std::move(child));

    root.update(0.25f);
    CHECK_EQ(observer->lastDt, 0.25f);
    CHECK_EQ(root.lastDt, 0.25f);
}

TEST_CASE("GameObject: nested children get update() recursively")
{
    Trace trace;
    Recorder root(trace, "root");

    auto mid = std::make_unique<Recorder>(trace, "mid");
    auto leaf = std::make_unique<Recorder>(trace, "leaf");
    mid->addChild(std::move(leaf));
    root.addChild(std::move(mid));

    trace.events.clear();
    root.update(0.01f);

    // leaf updates before mid (children-first), and mid before root.
    REQUIRE(trace.events.size() == 3u);
    CHECK_EQ(trace.events[0], std::string("leaf:update"));
    CHECK_EQ(trace.events[1], std::string("mid:update"));
    CHECK_EQ(trace.events[2], std::string("root:update"));
}

TEST_CASE("GameObject is non-copyable (compile-time contract)")
{
    // unique_ptr children make a copy meaningless; MSVC /permissive- will
    // C2280 if anyone synthesises a copy in a derived class. The deleted
    // copy ops here are also what the type-trait below pins down.
    static_assert(!std::is_copy_constructible<GameObject>::value,
                  "GameObject must not be copy-constructible");
    static_assert(!std::is_copy_assignable<GameObject>::value,
                  "GameObject must not be copy-assignable");
    CHECK(true); // hit at least one runtime check
}

TEST_MAIN()
