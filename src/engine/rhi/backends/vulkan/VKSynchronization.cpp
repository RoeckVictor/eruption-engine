#ifdef ERUPTION_VULKAN_SUPPORT

#include "VKSynchronization.h"
#include "VKCommon.h"
#include "VKDevice.h"

namespace engine::rhi {

// --- VKFence ---

VKFence::~VKFence() {
    if (m_fence && m_device) vkDestroyFence(m_device->device(), m_fence, nullptr);
}

bool VKFence::init(VKDevice* device) {
    m_device = device;
    VkFenceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    return VK_CHECK(vkCreateFence(device->device(), &info, nullptr, &m_fence));
}

bool VKFence::wait(uint64_t timeout_ns) {
    return vkWaitForFences(m_device->device(), 1, &m_fence, VK_TRUE, timeout_ns) == VK_SUCCESS;
}

void VKFence::reset() {
    vkResetFences(m_device->device(), 1, &m_fence);
}

bool VKFence::is_signaled() const {
    return vkGetFenceStatus(m_device->device(), m_fence) == VK_SUCCESS;
}

// --- VKEvent ---

VKEvent::~VKEvent() {
    if (m_event && m_device) vkDestroyEvent(m_device->device(), m_event, nullptr);
}

bool VKEvent::init(VKDevice* device) {
    m_device = device;
    VkEventCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
    return VK_CHECK(vkCreateEvent(device->device(), &info, nullptr, &m_event));
}

void VKEvent::set() { vkSetEvent(m_device->device(), m_event); }
void VKEvent::reset() { vkResetEvent(m_device->device(), m_event); }
bool VKEvent::is_set() const { return vkGetEventStatus(m_device->device(), m_event) == VK_EVENT_SET; }

// --- VKSemaphore ---

VKSemaphore::~VKSemaphore() {
    if (m_semaphore && m_device) vkDestroySemaphore(m_device->device(), m_semaphore, nullptr);
}

bool VKSemaphore::init(VKDevice* device) {
    m_device = device;
    VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    return VK_CHECK(vkCreateSemaphore(device->device(), &info, nullptr, &m_semaphore));
}

// --- VKTimelineSemaphore ---

VKTimelineSemaphore::~VKTimelineSemaphore() {
    if (m_semaphore && m_device) vkDestroySemaphore(m_device->device(), m_semaphore, nullptr);
}

bool VKTimelineSemaphore::init(VKDevice* device) {
    m_device = device;

    VkSemaphoreTypeCreateInfo timeline_info = {};
    timeline_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timeline_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timeline_info.initialValue = 0;

    VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = &timeline_info;

    return VK_CHECK(vkCreateSemaphore(device->device(), &info, nullptr, &m_semaphore));
}

uint64_t VKTimelineSemaphore::value() const {
    uint64_t val = 0;
    vkGetSemaphoreCounterValue(m_device->device(), m_semaphore, &val);
    return val;
}

void VKTimelineSemaphore::signal(uint64_t val) {
    VkSemaphoreSignalInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    info.semaphore = m_semaphore;
    info.value = val;
    vkSignalSemaphore(m_device->device(), &info);
}

bool VKTimelineSemaphore::wait(uint64_t val, uint64_t timeout_ns) {
    VkSemaphoreWaitInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    info.semaphoreCount = 1;
    info.pSemaphores = &m_semaphore;
    info.pValues = &val;
    return vkWaitSemaphores(m_device->device(), &info, timeout_ns) == VK_SUCCESS;
}

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
