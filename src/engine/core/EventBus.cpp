#include "engine/core/EventBus.h"

namespace engine {

void EventBus::unsubscribe(Handle handle) {
    // If we're currently publishing, defer the unsubscribe to avoid iterator invalidation
    if (m_is_publishing) {
        m_pending_unsubscribes.push_back(handle);
        return;
    }

    // Otherwise, unsubscribe immediately
    for (auto& [type, subs] : m_subscribers) {
        for (auto it = subs.begin(); it != subs.end(); ++it) {
            if (it->handle == handle) {
                subs.erase(it);
                return;
            }
        }
    }
}

void EventBus::clear() {
    m_subscribers.clear();
    m_pending_unsubscribes.clear();
}

void EventBus::process_pending_unsubscribes() {
    if (m_pending_unsubscribes.empty()) return;

    // Process all pending unsubscribes
    for (Handle handle : m_pending_unsubscribes) {
        for (auto& [type, subs] : m_subscribers) {
            for (auto it = subs.begin(); it != subs.end(); ++it) {
                if (it->handle == handle) {
                    subs.erase(it);
                    break; // Move to next handle
                }
            }
        }
    }

    m_pending_unsubscribes.clear();
}

} // namespace engine
