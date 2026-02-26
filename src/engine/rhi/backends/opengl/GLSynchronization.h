#pragma once

#include "engine/rhi/RHISynchronization.h"
#include <cstdint>

namespace engine::rhi {

class GLFence : public RHIFence {
public:
    GLFence() = default;
    ~GLFence() override;

    GLFence(GLFence&& other) noexcept;
    GLFence& operator=(GLFence&& other) noexcept;

    bool init();
    void destroy();

    void insert();

    bool wait(uint64_t timeout_ns = UINT64_MAX) override;
    void reset() override;
    bool is_signaled() const override;

    void* native_handle() const { return m_sync; }

private:
    void* m_sync = nullptr;
};

// Limited support - uses a simple signaled flag since OpenGL
// doesn't have true event objects
class GLEvent : public RHIEvent {
public:
    GLEvent() = default;
    ~GLEvent() override = default;

    bool init();

    void set() override;
    void reset() override;
    bool is_set() const override;

private:
    bool m_signaled = false;
};

// No-op implementation - OpenGL has a single queue so semaphores
// are not needed for queue synchronization
class GLSemaphore : public RHISemaphore {
public:
    GLSemaphore() = default;
    ~GLSemaphore() override = default;

    bool init();
};

// No-op implementation - OpenGL doesn't support timeline semaphores
class GLTimelineSemaphore : public RHITimelineSemaphore {
public:
    GLTimelineSemaphore() = default;
    ~GLTimelineSemaphore() override = default;

    bool init();

    uint64_t value() const override;
    void signal(uint64_t value) override;
    bool wait(uint64_t value, uint64_t timeout_ns = UINT64_MAX) override;

private:
    uint64_t m_value = 0;
};

}
