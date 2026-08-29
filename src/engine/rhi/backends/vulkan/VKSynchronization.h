#pragma once

#ifdef ERUPTION_VULKAN_SUPPORT

#include "engine/rhi/RHISynchronization.h"
#include <vulkan/vulkan.h>

namespace engine::rhi {

class VKDevice;

class VKFence : public RHIFence {
public:
    VKFence() = default;
    ~VKFence() override;

    bool init(VKDevice* device);

    bool wait(uint64_t timeout_ns = UINT64_MAX) override;
    void reset() override;
    bool is_signaled() const override;
    void* native_handle() const { return m_fence; }

private:
    VKDevice* m_device = nullptr;
    VkFence m_fence = VK_NULL_HANDLE;
};

class VKEvent : public RHIEvent {
public:
    VKEvent() = default;
    ~VKEvent() override;

    bool init(VKDevice* device);

    void set() override;
    void reset() override;
    bool is_set() const override;

private:
    VKDevice* m_device = nullptr;
    VkEvent m_event = VK_NULL_HANDLE;
};

class VKSemaphore : public RHISemaphore {
public:
    VKSemaphore() = default;
    ~VKSemaphore() override;

    bool init(VKDevice* device);
    VkSemaphore handle() const { return m_semaphore; }

private:
    VKDevice* m_device = nullptr;
    VkSemaphore m_semaphore = VK_NULL_HANDLE;
};

class VKTimelineSemaphore : public RHITimelineSemaphore {
public:
    VKTimelineSemaphore() = default;
    ~VKTimelineSemaphore() override;

    bool init(VKDevice* device);

    uint64_t value() const override;
    void signal(uint64_t value) override;
    bool wait(uint64_t value, uint64_t timeout_ns = UINT64_MAX) override;

private:
    VKDevice* m_device = nullptr;
    VkSemaphore m_semaphore = VK_NULL_HANDLE;
};

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
