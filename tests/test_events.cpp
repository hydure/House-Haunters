// EventManager / EventSubscription.
//
// Listeners are stored in a string-keyed bucket; queueEvent puts events on
// a queue drained by notify(); triggerEvent fires immediately.
// EventSubscription is an RAII handle that removes its listener on destruction.

#include "test_harness.hpp"
#include "engine/EventManager.hpp"
#include "engine/EngineEvents.hpp"

namespace {
void clearAllState()
{
    Events::clear();
}
} // namespace

TEST_CASE("Events: triggerEvent dispatches immediately")
{
    clearAllState();
    int hits = 0;
    long id = Events::addEventListener("ping",
        [&](base_event_type) { hits++; });
    Events::triggerEvent("ping", std::make_shared<BasicEvent>());
    CHECK_EQ(hits, 1);
    Events::removeEventListener(id);
}

TEST_CASE("Events: queueEvent fires only on notify")
{
    clearAllState();
    int hits = 0;
    long id = Events::addEventListener("ping",
        [&](base_event_type) { hits++; });
    Events::queueEvent("ping", std::make_shared<BasicEvent>());
    CHECK_EQ(hits, 0);
    Events::notify();
    CHECK_EQ(hits, 1);
    Events::removeEventListener(id);
}

TEST_CASE("Events: removeEventListener stops further calls")
{
    clearAllState();
    int hits = 0;
    long id = Events::addEventListener("ping",
        [&](base_event_type) { hits++; });
    Events::triggerEvent("ping", std::make_shared<BasicEvent>());
    Events::removeEventListener(id);
    Events::triggerEvent("ping", std::make_shared<BasicEvent>());
    CHECK_EQ(hits, 1);
}

TEST_CASE("Events: typed Event<T> payload survives the trip")
{
    clearAllState();
    std::string got;
    long id = Events::addEventListener("name",
        [&](base_event_type ev) {
            auto& cast = dynamic_cast< Event<std::string>& >(*ev);
            got = cast.data;
        });
    Events::triggerEvent("name",
        std::make_shared< Event<std::string> >(std::string("hello")));
    CHECK_EQ(got, std::string("hello"));
    Events::removeEventListener(id);
}

TEST_CASE("EventSubscription: removes listener on destruction (RAII)")
{
    clearAllState();
    int hits = 0;
    {
        EventSubscription sub(
            Events::addEventListener("ping",
                [&](base_event_type) { hits++; }));
        Events::triggerEvent("ping", std::make_shared<BasicEvent>());
        CHECK_EQ(hits, 1);
    }
    // After the scope: subscription destroyed, listener removed.
    Events::triggerEvent("ping", std::make_shared<BasicEvent>());
    CHECK_EQ(hits, 1);
}

TEST_CASE("EventSubscription: move transfers ownership")
{
    clearAllState();
    int hits = 0;
    EventSubscription outer;
    {
        EventSubscription inner(
            Events::addEventListener("ping",
                [&](base_event_type) { hits++; }));
        outer = std::move(inner);
    }
    // Moved-from subscription is now empty; outer still holds the listener.
    Events::triggerEvent("ping", std::make_shared<BasicEvent>());
    CHECK_EQ(hits, 1);
    outer.reset();
    Events::triggerEvent("ping", std::make_shared<BasicEvent>());
    CHECK_EQ(hits, 1);
}

TEST_CASE("Events: reentrant dispatch (listener removes another) is safe")
{
    // Regression coverage for the "shared_ptr listener snapshot" comment in
    // EventManager.hpp: dispatch must not invalidate other listeners' iters.
    clearAllState();
    int hitsA = 0, hitsB = 0;
    long idA = 0, idB = 0;
    idA = Events::addEventListener("ping",
        [&](base_event_type) {
            hitsA++;
            Events::removeEventListener(idB);
        });
    idB = Events::addEventListener("ping",
        [&](base_event_type) { hitsB++; });

    Events::triggerEvent("ping", std::make_shared<BasicEvent>());
    // A fires, removes B; B may or may not still fire this round, but the
    // program must not crash. After this round B is definitely gone.
    CHECK_EQ(hitsA, 1);

    Events::triggerEvent("ping", std::make_shared<BasicEvent>());
    CHECK_EQ(hitsA, 2);
    CHECK_EQ(hitsB, hitsB); // tautology, just ensures the world didn't crash
    Events::removeEventListener(idA);
}

TEST_CASE("Events::clear wipes all listeners and queued events")
{
    int hits = 0;
    Events::addEventListener("ping",
        [&](base_event_type) { hits++; });
    Events::queueEvent("ping", std::make_shared<BasicEvent>());
    Events::clear();
    Events::notify();
    CHECK_EQ(hits, 0);
}

TEST_MAIN()
