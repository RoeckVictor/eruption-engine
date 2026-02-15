#include "engine/core/EventBus.h"
#include "engine/core/Logger.h"

namespace engine {

void EventBus::unsubscribe(Handle handle) {
    // If we're currently publishing, defer the unsubscribe to avoid iterator invalidation
    if (m_publish_depth > 0) {
        m_pending_unsubscribes.push_back(handle);
        return;
    }

    // O(1) lookup of event type for this handle
    auto type_it = m_handle_to_type.find(handle);
    if (type_it == m_handle_to_type.end()) {
        Logger::instance().warning("EventBus", "unsubscribe called with unknown handle %u", handle);
        return;
    }

    auto subs_it = m_subscribers.find(type_it->second);
    if (subs_it != m_subscribers.end()) {
        auto& subs = subs_it->second;
        for (auto it = subs.begin(); it != subs.end(); ++it) {
            if (it->handle == handle) {
                subs.erase(it);
                break;
            }
        }
    }

    m_handle_to_type.erase(type_it);
}

void EventBus::clear() {
    if (m_publish_depth > 0) {
        // Defer the clear until all nested publishes are done to avoid
        // invalidating the subscriber vectors that publish() is iterating.
        m_clear_pending = true;
        return;
    }
    m_subscribers.clear();
    m_handle_to_type.clear();
    m_pending_unsubscribes.clear();
}

void EventBus::process_pending_unsubscribes() {
    // If a clear was requested during publish, perform it now (supersedes individual unsubscribes)
    if (m_clear_pending) {
        m_clear_pending = false;
        m_subscribers.clear();
        m_handle_to_type.clear();
        m_pending_unsubscribes.clear();
        return;
    }

    if (m_pending_unsubscribes.empty()) return;

    for (Handle handle : m_pending_unsubscribes) {
        auto type_it = m_handle_to_type.find(handle);
        if (type_it == m_handle_to_type.end()) continue;

        auto subs_it = m_subscribers.find(type_it->second);
        if (subs_it != m_subscribers.end()) {
            auto& subs = subs_it->second;
            for (auto it = subs.begin(); it != subs.end(); ++it) {
                if (it->handle == handle) {
                    subs.erase(it);
                    break;
                }
            }
        }

        m_handle_to_type.erase(type_it);
    }

    m_pending_unsubscribes.clear();
}

} // namespace engine
