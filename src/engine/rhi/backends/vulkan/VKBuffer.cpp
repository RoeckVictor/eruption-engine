#ifdef ERUPTION_VULKAN_SUPPORT

#include "VKBuffer.h"
#include "VKCommon.h"
#include "VKDevice.h"

namespace engine::rhi {

static VkBufferUsageFlags to_vk_buffer_usage(BufferType type) {
    // All buffer types include TRANSFER_SRC so readback() works (vkCmdCopyBuffer requires it on the source).
    // All include TRANSFER_DST so staged uploads work.
    constexpr VkBufferUsageFlags transfer_bits = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    switch (type) {
        case BufferType::Vertex:    return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT  | transfer_bits;
        case BufferType::Index:     return VK_BUFFER_USAGE_INDEX_BUFFER_BIT   | transfer_bits;
        case BufferType::Uniform:   return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | transfer_bits;
        case BufferType::Storage:   return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | transfer_bits;
        case BufferType::PixelPack: return transfer_bits;
        default:                    return transfer_bits;
    }
}

VKBuffer::~VKBuffer() {
    if (m_buffer && m_device) {
        if (m_manually_mapped) {
            vmaUnmapMemory(m_device->allocator(), m_allocation);
            m_manually_mapped = false;
        }
        // Defer destruction — buffer may still be referenced by in-flight command buffers.
        VmaAllocator alloc = m_device->allocator();
        VkBuffer buf = m_buffer;
        VmaAllocation allocation = m_allocation;
        m_device->defer_deletion([alloc, buf, allocation]() {
            vmaDestroyBuffer(alloc, buf, allocation);
        });
        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
}

bool VKBuffer::init(VKDevice* device, const BufferDesc& desc) {
    m_device = device;
    m_type = desc.type;
    m_usage = desc.usage;
    m_size = desc.size > 0 ? desc.size : 1;

    VkBufferCreateInfo buf_info = {};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.size = m_size;
    buf_info.usage = to_vk_buffer_usage(desc.type);
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info = {};
    if (desc.usage == BufferUsage::Dynamic || desc.usage == BufferUsage::Stream) {
        // Host-visible for frequent CPU writes
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                           VMA_ALLOCATION_CREATE_MAPPED_BIT;
    } else {
        // GPU-only for static data
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    }

    VmaAllocationInfo result_info = {};
    if (!VK_CHECK(vmaCreateBuffer(device->allocator(), &buf_info, &alloc_info,
                                  &m_buffer, &m_allocation, &result_info))) {
        return false;
    }

    // For dynamic/stream buffers, keep persistently mapped
    if (desc.usage == BufferUsage::Dynamic || desc.usage == BufferUsage::Stream) {
        m_mapped = result_info.pMappedData;
    }

    // Upload initial data
    if (desc.initial_data && desc.size > 0) {
        if (m_mapped) {
            memcpy(m_mapped, desc.initial_data, desc.size);
            vmaFlushAllocation(device->allocator(), m_allocation, 0, desc.size);
        } else {
            device->upload_buffer_staged(m_buffer, 0, desc.size, desc.initial_data);
        }
    }

    m_valid = true;
    return true;
}

void VKBuffer::update(size_t offset, size_t size, const void* data) {
    if (!data || !m_valid) return;

    if (m_mapped) {
        memcpy(static_cast<uint8_t*>(m_mapped) + offset, data, size);
        vmaFlushAllocation(m_device->allocator(), m_allocation, offset, size);
    } else {
        m_device->upload_buffer_staged(m_buffer, offset, size, data);
    }
}

bool VKBuffer::resize(size_t new_size, const void* new_data) {
    if (!m_device) return false;

    // Unmap if manually mapped
    if (m_manually_mapped) {
        vmaUnmapMemory(m_device->allocator(), m_allocation);
        m_manually_mapped = false;
    }
    m_mapped = nullptr;

    // Defer destruction of old buffer (may still be in-flight on GPU)
    VmaAllocator alloc = m_device->allocator();
    VkBuffer old_buffer = m_buffer;
    VmaAllocation old_allocation = m_allocation;
    if (old_buffer && old_allocation) {
        m_device->defer_deletion([alloc, old_buffer, old_allocation]() {
            vmaDestroyBuffer(alloc, old_buffer, old_allocation);
        });
    }
    m_buffer = VK_NULL_HANDLE;
    m_allocation = VK_NULL_HANDLE;

    BufferDesc desc;
    desc.type = m_type;
    desc.usage = m_usage;
    desc.size = new_size;
    desc.initial_data = new_data;
    return init(m_device, desc);
}

bool VKBuffer::readback(size_t offset, size_t size, void* dst) const {
    if (!dst || !m_valid) return false;

    if (m_mapped) {
        vmaInvalidateAllocation(m_device->allocator(), m_allocation, offset, size);
        memcpy(dst, static_cast<const uint8_t*>(m_mapped) + offset, size);
        return true;
    }

    m_device->readback_buffer_staged(m_buffer, offset, size, dst);
    return true;
}

void* VKBuffer::map_read(size_t offset, size_t size) {
    if (m_mapped) {
        // Persistently mapped — invalidate the requested range and return offset pointer
        vmaInvalidateAllocation(m_device->allocator(), m_allocation, offset, size);
        return static_cast<uint8_t*>(m_mapped) + offset;
    }

    // For GPU-only buffers, attempt a direct map first (works if memory happens to be host-visible)
    void* data = nullptr;
    if (vmaMapMemory(m_device->allocator(), m_allocation, &data) == VK_SUCCESS) {
        m_manually_mapped = true;
        vmaInvalidateAllocation(m_device->allocator(), m_allocation, offset, size);
        return static_cast<uint8_t*>(data) + offset;
    }

    // Device-local memory that isn't host-visible: fall back to staging readback.
    // Allocate a temporary host-visible buffer to hold the mapped data.
    m_readback_staging = std::make_unique<uint8_t[]>(size);
    m_readback_staging_size = size;
    m_device->readback_buffer_staged(m_buffer, offset, size, m_readback_staging.get());
    m_manually_mapped = true;
    return m_readback_staging.get();
}

void VKBuffer::unmap() {
    if (!m_manually_mapped) return;

    if (m_readback_staging) {
        // Was using staging fallback — just release the staging memory
        m_readback_staging.reset();
        m_readback_staging_size = 0;
    } else {
        // Was using direct VMA map
        vmaUnmapMemory(m_device->allocator(), m_allocation);
    }
    m_manually_mapped = false;
}

void VKBuffer::bind(uint32_t /*slot*/) {
    // In Vulkan, binding happens through context commands, not on the buffer itself
}

} // namespace engine::rhi

#endif // ERUPTION_VULKAN_SUPPORT
