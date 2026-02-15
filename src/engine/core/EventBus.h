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
///
/// Thread safety: NOT thread-safe. All subscribe/publish/unsubscribe calls
/// must happen on the same thread (the main/update thread).
class EventBus {
public:
    using Handle = uint32_t;

    /// Subscribe to events of type E.  Returns a handle for unsubscribe().
    template<typename E>
    Handle subscribe(std::function<void(const E&)> callback) {
        Handle h = m_next_handle++;
        auto type = std::type_index(typeid(E));
        auto& subs = m_subscribers[type];
        subs.push_back({h, [cb = std::move(callback)](const void* ptr) {
            cb(*static_cast<const E*>(ptr));
        }});
        m_handle_to_type.emplace(h, type);
        return h;
    }

    /// Maximum recursion depth for nested publish calls.
    static constexpr int MAX_PUBLISH_DEPTH = 16;

    /// Publish an event to all subscribers of type E.
    template<typename E>
    void publish(const E& event) {
        auto it = m_subscribers.find(std::type_index(typeid(E)));
        if (it == m_subscribers.end()) return;

        // Depth counter supports recursive publishes (callback publishing another event)
        ++m_publish_depth;
        if (m_publish_depth > MAX_PUBLISH_DEPTH) {
            --m_publish_depth;
            return;  // Prevent infinite recursion from callback loops
        }
        // Iterate by index: safe if callbacks add new subscribers (vector may grow).
        // Wrapped in try-catch to guarantee m_publish_depth is decremented even
        // if a callback throws, preventing the EventBus from getting permanently stuck.
        auto& subs = it->second;
        size_t count = subs.size();
        try {
            for (size_t i = 0; i < count; ++i) {
                subs[i].callback(&event);
            }
        } catch (...) {
            --m_publish_depth;
            if (m_publish_depth == 0) {
                process_pending_unsubscribes();
            }
            throw;
        }
        --m_publish_depth;

        // Only process pending unsubscribes when all nested publishes are done
        if (m_publish_depth == 0) {
            process_pending_unsubscribes();
        }
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
    std::unordered_map<Handle, std::type_index> m_handle_to_type; // O(1) handle->event type lookup
    std::vector<Handle> m_pending_unsubscribes;
    Handle m_next_handle = 1;
    int m_publish_depth = 0;
    bool m_clear_pending = false;  // Deferred clear requested during publish
};

} // namespace engine
