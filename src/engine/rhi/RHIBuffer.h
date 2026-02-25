#pragma once

#include "RHITypes.h"

namespace engine::rhi {

/// Abstract buffer resource (vertex, index, uniform, storage)
class RHIBuffer {
public:
    virtual ~RHIBuffer() = default;

    // Non-copyable
    RHIBuffer(const RHIBuffer&) = delete;
    RHIBuffer& operator=(const RHIBuffer&) = delete;

    /// Update a region of the buffer with new data
    /// @param offset Byte offset into the buffer
    /// @param size Number of bytes to update
    /// @param data Source data pointer
    virtual void update(size_t offset, size_t size, const void* data) = 0;

    /// Resize the buffer (reallocates GPU memory)
    /// @param new_size New buffer size in bytes
    /// @param new_data Optional data to upload (may be nullptr)
    /// @return true if resize succeeded
    virtual bool resize(size_t new_size, const void* new_data = nullptr) = 0;

    /// Read data back from the buffer to CPU memory (synchronous)
    /// @param offset Byte offset into the buffer
    /// @param size Number of bytes to read
    /// @param dst Destination buffer (must be at least size bytes)
    /// @return true if readback succeeded
    virtual bool readback(size_t offset, size_t size, void* dst) const = 0;

    /// Map buffer memory for CPU reading (for async readback flow)
    /// Use this after async copy commands to access the data.
    /// @param offset Byte offset into the buffer
    /// @param size Number of bytes to map
    /// @return Pointer to mapped memory, or nullptr on failure
    virtual void* map_read(size_t offset, size_t size) = 0;

    /// Unmap previously mapped buffer memory
    virtual void unmap() = 0;

    /// Bind this buffer to a slot (for storage/uniform buffers)
    /// @param slot Binding point index
    virtual void bind(uint32_t slot) = 0;

    /// Get the buffer's native handle (backend-specific)
    /// For OpenGL: GLuint buffer ID
    /// For Vulkan: VkBuffer
    virtual void* native_handle() const = 0;

    // Accessors
    BufferType type() const { return m_type; }
    BufferUsage usage() const { return m_usage; }
    size_t size() const { return m_size; }
    bool valid() const { return m_valid; }

protected:
    RHIBuffer() = default;

    BufferType m_type = BufferType::Vertex;
    BufferUsage m_usage = BufferUsage::Static;
    size_t m_size = 0;
    bool m_valid = false;
};

} // namespace engine::rhi
