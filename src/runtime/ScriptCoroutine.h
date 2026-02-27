#pragma once

#include <coroutine>
#include <functional>
#include <variant>
#include <cstdint>
#include <entt/entt.hpp>

namespace runtime {

using CoroutineHandle = uint32_t;

class Coroutine;

struct WaitForSeconds {
    float seconds;
    explicit WaitForSeconds(float s) : seconds(s) {}
};

struct WaitForNextFrame {};

struct WaitUntil {
    std::function<bool()> condition;
    explicit WaitUntil(std::function<bool()> cond) : condition(std::move(cond)) {}
};

struct WaitWhile {
    std::function<bool()> condition;
    explicit WaitWhile(std::function<bool()> cond) : condition(std::move(cond)) {}
};

using YieldInstruction = std::variant<
    std::monostate,
    WaitForSeconds,
    WaitForNextFrame,
    WaitUntil,
    WaitWhile
>;

// C++20 coroutine return type for script coroutines
// Provides Unity-like coroutine semantics with co_yield
class Coroutine {
public:
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

        std::suspend_always yield_value(WaitForSeconds instruction) {
            current_yield = instruction;
            return {};
        }

        std::suspend_always yield_value(WaitForNextFrame instruction) {
            current_yield = instruction;
            return {};
        }

        std::suspend_always yield_value(WaitUntil instruction) {
            current_yield = std::move(instruction);
            return {};
        }

        std::suspend_always yield_value(WaitWhile instruction) {
            current_yield = std::move(instruction);
            return {};
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    Coroutine() = default;

    explicit Coroutine(handle_type h) : m_handle(h) {}

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

    bool done() const {
        return !m_handle || m_handle.done();
    }

    void resume() {
        if (m_handle && !m_handle.done()) {
            m_handle.resume();
            // Check for exceptions
            if (m_handle.promise().exception) {
                std::rethrow_exception(m_handle.promise().exception);
            }
        }
    }

    const YieldInstruction& current_yield() const {
        static YieldInstruction empty;
        return m_handle ? m_handle.promise().current_yield : empty;
    }

    bool valid() const { return m_handle != nullptr; }

    handle_type release() {
        auto h = m_handle;
        m_handle = nullptr;
        return h;
    }

private:
    handle_type m_handle = nullptr;
};

struct CoroutineInstance {
    CoroutineHandle handle = 0;
    entt::entity owner = entt::null;
    Coroutine::handle_type coro_handle = nullptr;
    YieldInstruction current_yield;
    float wait_timer = 0.0f;
    bool active = true;
    bool started = false;
};

} // namespace runtime
