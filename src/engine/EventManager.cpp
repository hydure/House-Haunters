#include "engine/EventManager.hpp"

// define static members
long Events::next_listener_id = 0;
std::map< std::string, event_list > Events::listeners_map;
std::unordered_map<long, EventListenerLocation> Events::id_index;
std::queue< base_event_type > Events::events;

namespace {
// Dispatch on a copy of the listener list so reentrant removeEventListener /
// clearAll calls during a fired callback don't invalidate the iteration.
void dispatch(const event_list& listeners, const base_event_type& e)
{
    event_list snapshot = listeners;
    for (const auto& entry : snapshot) {
        if (entry && entry->active) {
            entry->fn(e);
        }
    }
}
} // namespace

long Events::addEventListener(std::string type, std::function<void (base_event_type)> listener)
{
    auto entry = std::make_shared<EventListenerEntry>();
    entry->id = ++next_listener_id;
    entry->active = true;
    entry->fn = std::move(listener);

    // Push into the bucket and store the position so removeEventListener
    // can later splice it out in O(1). std::list iterators are stable.
    auto& bucket = listeners_map[type];
    bucket.push_back(entry);
    auto it = bucket.end();
    --it;
    id_index.emplace(entry->id, EventListenerLocation{std::move(type), it});
    return entry->id;
}

void Events::queueEvent(std::string type, base_event_type e)
{
    e->setEventType(type);
    events.push(std::move(e));
}

void Events::triggerEvent(std::string type, base_event_type e)
{
    e->setEventType(type);
    auto it = listeners_map.find(type);
    if (it == listeners_map.end()) {
        return;
    }
    dispatch(it->second, e);
}

void Events::notify()
{
    while (!events.empty()) {
        auto e = events.front();
        events.pop();
        auto it = listeners_map.find(e->getEventType());
        if (it == listeners_map.end()) {
            continue;
        }
        dispatch(it->second, e);
    }
}

void Events::removeEventListener(long id)
{
    // O(1) via id_index instead of scanning every bucket.
    auto idx_it = id_index.find(id);
    if (idx_it == id_index.end()) {
        return;
    }
    EventListenerLocation loc = idx_it->second;
    id_index.erase(idx_it);

    auto bucket_it = listeners_map.find(loc.type);
    if (bucket_it == listeners_map.end()) {
        return;
    }
    auto& list = bucket_it->second;
    if (*loc.iter) {
        (*loc.iter)->active = false; // skip in any in-flight dispatch snapshot
    }
    list.erase(loc.iter);
}

void Events::clearAll(const std::string& s)
{
    auto it = listeners_map.find(s);
    if (it == listeners_map.end()) {
        return;
    }
    // Deactivate + drop from id_index first so any later removeEventListener
    // calls (e.g. from RAII handles outliving the listeners) are no-ops, and
    // any in-flight dispatch snapshot skips these entries.
    for (auto& entry : it->second) {
        if (entry) {
            entry->active = false;
            id_index.erase(entry->id);
        }
    }
    it->second.clear();
}

void Events::clear()
{
    for (auto& kv : listeners_map) {
        for (auto& entry : kv.second) {
            if (entry) entry->active = false;
        }
    }
    listeners_map.clear();
    id_index.clear();
    events = std::queue<base_event_type>();
}