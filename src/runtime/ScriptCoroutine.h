#pragma once

#include <coroutine>
#include <functional>
#include <variant>
#include <cstdint>
#include <entt/entt.hpp>

namespace runtime {

/// Handle for managing coroutines from scripts.
using CoroutineHandle = uint32_t;

// Forward declarations
class Coroutine;

/// Wait for a specified number of seconds before resuming.
struct WaitForSeconds {
    float seconds;
    explicit WaitForSeconds(float s) : seconds(s) {}
};

/// Wait until the next frame before resuming.
struct WaitForNextFrame {};

/// Wait until a condition becomes true before resuming.
struct WaitUntil {
    std::function<bool()> condition;
    explicit WaitUntil(std::function<bool()> cond) : condition(std::move(cond)) {}
};

/// Wait while a condition is true (inverse of WaitUntil).
struct WaitWhile {
    std::function<bool()> condition;
    explicit WaitWhile(std::function<bool()> cond) : condition(std::move(cond)) {}
};

/// Variant holding any yield instruction type.
using YieldInstruction = std::variant<
    std::monostate,     // Default: wait for next frame
    WaitForSeconds,
    WaitForNextFrame,
    WaitUntil,
    WaitWhile
>;

/// C++20 coroutine return type for script coroutines.
/// Provides Unity-like coroutine semantics with co_yield.
///
/// Example usage:
/// ```cpp
/// Coroutine fade_out() {
///     for (float t = 1.0f; t > 0.0f; t -= 0.02f) {
///         set_opacity(t);
///         co_yield WaitForNextFrame();
///     }
/// }
/// ```
class Coroutine {
public:
    /// Promise type required by C++20 coroutines.
    struct promise_type {
        YieldInstruction current_yield;
        std::exception_ptr exception;

        Coroutine get_return_object() {
            return Coroutine{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_void() {}

        void unhandled_exception() {
            exception = std::current_exception();
        }

        /// Yield with WaitForSeconds.
        std::suspend_always yield_value(WaitForSeconds instruction) {
            current_yield = instruction;
            return {};
        }

        /// Yield with WaitForNextFrame.
        std::suspend_always yield_value(WaitForNextFrame instruction) {
            current_yield = instruction;
            return {};
        }

        /// Yield with WaitUntil.
        std::suspend_always yield_value(WaitUntil instruction) {
            current_yield = std::move(instruction);
            return {};
        }

        /// Yield with WaitWhile.
        std::suspend_always yield_value(WaitWhile instruction) {
            current_yield = std::move(instruction);
            return {};
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    Coroutine() = default;

    explicit Coroutine(handle_type h) : m_handle(h) {}

    // Move-only semantics
    Coroutine(const Coroutine&) = delete;
    Coroutine& operator=(const Coroutine&) = delete;

    Coroutine(Coroutine&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }

    Coroutine& operator=(Coroutine&& other) noexcept {
        if (this != &other) {
            if (m_handle) m_handle.destroy();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    ~Coroutine() {
        if (m_handle) {
            m_handle.destroy();
        }
    }

    /// Check if the coroutine has finished execution.
    bool done() const {
        return !m_handle || m_handle.done();
    }

    /// Resume the coroutine. Should only be called when ready to resume.
    void resume() {
        if (m_handle && !m_handle.done()) {
            m_handle.resume();
            // Check for exceptions
            if (m_handle.promise().exception) {
                std::rethrow_exception(m_handle.promise().exception);
            }
        }
    }

    /// Get the current yield instruction (what the coroutine is waiting for).
    const YieldInstruction& current_yield() const {
        static YieldInstruction empty;
        return m_handle ? m_handle.promise().current_yield : empty;
    }

    /// Check if the coroutine handle is valid.
    bool valid() const { return m_handle != nullptr; }

    /// Release ownership of the handle (for transfer to runtime).
    handle_type release() {
        auto h = m_handle;
        m_handle = nullptr;
        return h;
    }

private:
    handle_type m_handle = nullptr;
};

/// Internal structure for tracking active coroutines in RuntimeContext.
struct CoroutineInstance {
    CoroutineHandle handle = 0;                          // User-facing handle
    entt::entity owner = entt::null;                     // Owning entity
    Coroutine::handle_type coro_handle = nullptr;        // C++ coroutine handle
    YieldInstruction current_yield;                      // Current wait state
    float wait_timer = 0.0f;                             // Timer for WaitForSeconds
    bool active = true;                                  // Still running?
    bool started = false;                                // Has been resumed at least once?
};

} // namespace runtime
