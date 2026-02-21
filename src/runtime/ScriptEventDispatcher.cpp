#include "ScriptEventDispatcher.h"
#include <algorithm>

namespace runtime {

EventHandle ScriptEventDispatcher::subscribe(const char* event_name, entt::entity owner, Callback callback) {
    if (!event_name || !callback) return 0;

    EventHandle handle = m_next_handle++;
    std::string name(event_name);

    m_subscriptions[name].push_back({handle, owner, callback});
    m_handle_to_event.emplace(handle, name);

    return handle;
}

void ScriptEventDispatcher::unsubscribe(EventHandle handle) {
    if (handle == 0) return;

    // If currently dispatching, defer the unsubscribe
    if (m_dispatch_depth > 0) {
        m_pending_unsubscribes.push_back(handle);
        return;
    }

    // Find which event this handle belongs to
    auto it = m_handle_to_event.find(handle);
    if (it == m_handle_to_event.end()) return;

    const std::string& event_name = it->second;

    // Remove from subscriptions
    auto subs_it = m_subscriptions.find(event_name);
    if (subs_it != m_subscriptions.end()) {
        auto& subs = subs_it->second;
        subs.erase(
            std::remove_if(subs.begin(), subs.end(),
                [handle](const Subscription& s) { return s.handle == handle; }),
            subs.end());

        // Clean up empty subscription lists
        if (subs.empty()) {
            m_subscriptions.erase(subs_it);
        }
    }

    m_handle_to_event.erase(it);
}

void ScriptEventDispatcher::dispatch(const char* event_name, const EventData* data) {
    if (!event_name) return;

    auto it = m_subscriptions.find(event_name);
    if (it == m_subscriptions.end()) return;

    // Recursion depth protection
    ++m_dispatch_depth;
    if (m_dispatch_depth > MAX_DISPATCH_DEPTH) {
        --m_dispatch_depth;
        return;
    }

    // Provide empty data if none given
    EventData empty_data;
    const EventData& event_data = data ? *data : empty_data;

    // Iterate by index: safe if callbacks add new subscribers (vector may grow)
    auto& subs = it->second;
    size_t count = subs.size();

    try {
        for (size_t i = 0; i < count; ++i) {
            if (subs[i].callback) {
                subs[i].callback(event_data);
            }
        }
    } catch (...) {
        --m_dispatch_depth;
        if (m_dispatch_depth == 0) {
            process_pending_unsubscribes();
        }
        throw;
    }

    --m_dispatch_depth;

    // Process pending unsubscribes when all nested dispatches are done
    if (m_dispatch_depth == 0) {
        process_pending_unsubscribes();
    }
}

void ScriptEventDispatcher::cleanup_entity(entt::entity entity) {
    // Collect handles to unsubscribe
    std::vector<EventHandle> to_remove;

    for (const auto& [event_name, subs] : m_subscriptions) {
        for (const auto& sub : subs) {
            if (sub.owner == entity) {
                to_remove.push_back(sub.handle);
            }
        }
    }

    // Unsubscribe all (handles deferred if dispatching)
    for (EventHandle handle : to_remove) {
        unsubscribe(handle);
    }
}

void ScriptEventDispatcher::clear() {
    // If currently dispatching, defer the clear
    if (m_dispatch_depth > 0) {
        m_clear_pending = true;
        return;
    }

    m_subscriptions.clear();
    m_handle_to_event.clear();
    m_pending_unsubscribes.clear();
    m_next_handle = 1;
}

void ScriptEventDispatcher::process_pending_unsubscribes() {
    // Handle deferred clear
    if (m_clear_pending) {
        m_clear_pending = false;
        m_subscriptions.clear();
        m_handle_to_event.clear();
        m_pending_unsubscribes.clear();
        m_next_handle = 1;
        return;
    }

    // Process deferred unsubscribes
    for (EventHandle handle : m_pending_unsubscribes) {
        auto it = m_handle_to_event.find(handle);
        if (it == m_handle_to_event.end()) continue;

        const std::string& event_name = it->second;

        auto subs_it = m_subscriptions.find(event_name);
        if (subs_it != m_subscriptions.end()) {
            auto& subs = subs_it->second;
            subs.erase(
                std::remove_if(subs.begin(), subs.end(),
                    [handle](const Subscription& s) { return s.handle == handle; }),
                subs.end());

            if (subs.empty()) {
                m_subscriptions.erase(subs_it);
            }
        }

        m_handle_to_event.erase(it);
    }

    m_pending_unsubscribes.clear();
}

}
