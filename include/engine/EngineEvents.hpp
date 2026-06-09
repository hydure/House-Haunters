#ifndef ENGINE_EVENTS_HPP
#define ENGINE_EVENTS_HPP

#include <string>

class BasicEvent {
public:
    BasicEvent() = default;
    virtual ~BasicEvent() = default;
    const std::string& getEventType() const { return eventType; }
    void setEventType(const std::string& t) { eventType = t; }
protected:
    std::string eventType;
};

template <class Data_T>
class Event : public BasicEvent {
public:
    explicit Event(Data_T d) : data(std::move(d)) {}
    Data_T data;
};

#endif