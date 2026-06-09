#ifndef EVENTS_MANAGER_HPP
#define EVENTS_MANAGER_HPP
//////////////////////////
// Pretty basic string-keyed event manager.
//
// addEventListener returns a numeric id. Subscribers should normally hold the
// id inside an EventSubscription RAII handle (below), which removes the
// listener automatically when the handle goes out of scope. Dispatch is
// reentrancy-safe: a listener that fires removeEventListener / clearAll while
// being dispatched will not cause use-after-free on other listeners in the
// same dispatch loop.
/////////////////////////
#include <queue>
#include <list>
#include <map>
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include "engine/EngineEvents.hpp"

// We have to use a shared_ptr to prevent object slicing
typedef std::shared_ptr<BasicEvent> base_event_type;

struct EventListenerEntry {
    long id = 0;
    bool active = false;
    std::function<void (base_event_type)> fn;
};
// shared_ptr so a dispatch loop can hold a snapshot while listeners are
// removed concurrently from the live list.
typedef std::list< std::shared_ptr<EventListenerEntry> > event_list;

// Where in the listener tables a given id lives. Stored in id_index so
// removeEventListener can find and erase in O(1) instead of scanning every
// bucket for the right id.
struct EventListenerLocation {
    std::string type;
    event_list::iterator iter;
};

class Events
{
public:
    static long addEventListener(std::string type, std::function<void (base_event_type)> listener);
    static void removeEventListener(long id);
    static void queueEvent(std::string type, base_event_type e);
    // Triggers listeners immediately rather than waiting for the next notify().
    static void triggerEvent(std::string type, base_event_type e);
    static void clearAll(const std::string& s);
    static void clearEvent() { events = std::queue<base_event_type>(); }
    // Wipes every listener + pending event. Useful between games / on
    // shutdown so dangling captures don't reference freed game state.
    static void clear();
    static void notify();
private:
    static long next_listener_id;
    static std::map<std::string, event_list> listeners_map;
    static std::unordered_map<long, EventListenerLocation> id_index;
    static std::queue<base_event_type> events;
};

// RAII handle that removes its registered listener on destruction.
// Non-copyable, movable. Empty handles are a no-op.
class EventSubscription
{
public:
    EventSubscription() = default;
    explicit EventSubscription(long id) : id_(id), active_(true) {}
    EventSubscription(const EventSubscription&) = delete;
    EventSubscription& operator=(const EventSubscription&) = delete;
    EventSubscription(EventSubscription&& other) noexcept
        : id_(other.id_), active_(other.active_) { other.active_ = false; }
    EventSubscription& operator=(EventSubscription&& other) noexcept
    {
        if (this != &other) {
            reset();
            id_ = other.id_;
            active_ = other.active_;
            other.active_ = false;
        }
        return *this;
    }
    ~EventSubscription() { reset(); }
    void reset()
    {
        if (active_) {
            Events::removeEventListener(id_);
            active_ = false;
        }
    }
private:
    long id_ = 0;
    bool active_ = false;
};

#endif
