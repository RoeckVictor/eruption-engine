#pragma once

#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace engine {

/// Typed event bus with immediate synchronous dispatch.
///
/// Systems publish events via publish<E>(event) and subscribe via
/// subscribe<E>(callback). Subscribers receive a const reference to
/// the event object. Subscriptions return a handle for unsubscribe().
///
/// All dispatch is synchronous: publish() calls every subscriber for
/// that event type inline before returning.
class EventBus {
public:
    using Handle = uint32_t;

    /// Subscribe to events of type E.  Returns a handle for unsubscribe().
    template<typename E>
    Handle subscribe(std::function<void(const E&)> callback) {
        Handle h = m_next_handle++;
        auto& subs = m_subscribers[std::type_index(typeid(E))];
        subs.push_back({h, [cb = std::move(callback)](const void* ptr) {
            cb(*static_cast<const E*>(ptr));
        }});
        return h;
    }

    /// Publish an event to all subscribers of type E.
    template<typename E>
    void publish(const E& event) {
        auto it = m_subscribers.find(std::type_index(typeid(E)));
        if (it == m_subscribers.end()) return;

        // Prevent unsubscribes during iteration
        m_is_publishing = true;
        for (auto& sub : it->second) {
            sub.callback(&event);
        }
        m_is_publishing = false;

        // Process any pending unsubscribes
        process_pending_unsubscribes();
    }

    /// Remove a subscription by handle.
    void unsubscribe(Handle handle);

    /// Remove all subscriptions.
    void clear();

private:
    struct Subscription {
        Handle handle;
        std::function<void(const void*)> callback;
    };

    /// Process any pending unsubscribes after publishing.
    void process_pending_unsubscribes();

    std::unordered_map<std::type_index, std::vector<Subscription>> m_subscribers;
    std::vector<Handle> m_pending_unsubscribes;
    Handle m_next_handle = 1;
    bool m_is_publishing = false;
};

} // namespace engine
