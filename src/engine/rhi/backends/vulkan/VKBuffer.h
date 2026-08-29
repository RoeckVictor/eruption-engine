#pragma once

#ifdef ERUPTION_VULKAN_SUPPORT

#include "engine/rhi/RHIBuffer.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <memory>

namespace engine::rhi {

class VKDevice;

class VKBuffer : public RHIBuffer {
public:
    VKBuffer() = default;
    ~VKBuffer() override;

    bool init(VKDevice* device, const BufferDesc& desc);

    void update(size_t offset, size_t size, const void* data) override;
    bool resize(size_t new_size, const void* new_data = nullptr) override;
    bool readback(size_t offset, size_t size, void* dst) const override;
    void* map_read(size_t offset, size_t size) override;
    void unmap() override;
    void bind(uint32_t slot) override;
    void* native_handle() const override { return m_buffer; }

    VkBuffer vk_buffer() const { return m_buffer; }

private:
    VKDevice* m_device = nullptr;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    void* m_mapped = nullptr;        // Non-null if persistently mapped (dynamic/stream buffers)
    bool m_manually_mapped = false;  // True between map_read() and unmap() calls

    // Fallback staging buffer for map_read on device-local (non-host-visible) memory
    std::unique_ptr<uint8_t[]> m_readback_staging;
    size_t m_readback_staging_size = 0;
};

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
