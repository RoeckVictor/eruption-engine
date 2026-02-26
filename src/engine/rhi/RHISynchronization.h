#pragma once

#include <cstdint>

namespace engine::rhi {

// Fence for CPU-GPU synchronization
// Used to wait for GPU operations to complete before CPU reads results
class RHIFence {
public:
    virtual ~RHIFence() = default;

    RHIFence(const RHIFence&) = delete;
    RHIFence& operator=(const RHIFence&) = delete;

    virtual bool wait(uint64_t timeout_ns = UINT64_MAX) = 0;

    virtual void reset() = 0;

    virtual bool is_signaled() const = 0;

protected:
    RHIFence() = default;
};

// Event for fine-grained GPU synchronization.
// Can be set/reset from both CPU and GPU.
class RHIEvent {
public:
    virtual ~RHIEvent() = default;

    RHIEvent(const RHIEvent&) = delete;
    RHIEvent& operator=(const RHIEvent&) = delete;

    virtual void set() = 0;

    virtual void reset() = 0;

    virtual bool is_set() const = 0;

protected:
    RHIEvent() = default;
};

// Semaphore for GPU-GPU synchronization between queues
// Used for synchronizing work between different command queues
class RHISemaphore {
public:
    virtual ~RHISemaphore() = default;

    RHISemaphore(const RHISemaphore&) = delete;
    RHISemaphore& operator=(const RHISemaphore&) = delete;

protected:
    RHISemaphore() = default;
};

// Timeline semaphore for ordered GPU-GPU synchronization.
// OpenGL: Not supported.
class RHITimelineSemaphore {
public:
    virtual ~RHITimelineSemaphore() = default;

    RHITimelineSemaphore(const RHITimelineSemaphore&) = delete;
    RHITimelineSemaphore& operator=(const RHITimelineSemaphore&) = delete;

    virtual uint64_t value() const = 0;

    virtual void signal(uint64_t value) = 0;

    virtual bool wait(uint64_t value, uint64_t timeout_ns = UINT64_MAX) = 0;

protected:
    RHITimelineSemaphore() = default;
};

}
