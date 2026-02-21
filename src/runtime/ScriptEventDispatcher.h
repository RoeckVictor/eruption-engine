#pragma once

#include "ScriptEvents.h"
#include <entt/entt.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace runtime {
class ScriptEventDispatcher {
public:
    // Callback signature for event handlers (DLL-safe raw function pointer)
    using Callback = void(*)(const EventData&);

    EventHandle subscribe(const char* event_name, entt::entity owner, Callback callback);
    void unsubscribe(EventHandle handle);
    void dispatch(const char* event_name, const EventData* data);
    void cleanup_entity(entt::entity entity);
    void clear();

    static constexpr int MAX_DISPATCH_DEPTH = 16;

private:
    struct Subscription {
        EventHandle handle;
        entt::entity owner;
        Callback callback;
    };

    void process_pending_unsubscribes();

    std::unordered_map<std::string, std::vector<Subscription>> m_subscriptions;
    std::unordered_map<EventHandle, std::string> m_handle_to_event;
    std::vector<EventHandle> m_pending_unsubscribes;
    EventHandle m_next_handle = 1;
    int m_dispatch_depth = 0;
    bool m_clear_pending = false;
};

}
