#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include "engine/rhi/RHIBuffer.h"

namespace engine::graphics {

enum class BufferUsage {
    StaticDraw,
    DynamicDraw,
    StreamRead,  // For readback-friendly buffers
};

/// Shader storage buffer wrapper that delegates to RHI
/// @note For new code, consider using engine::rhi::RHIBuffer directly
class ShaderStorageBuffer {
public:
    ShaderStorageBuffer() = default;
    ~ShaderStorageBuffer();

    ShaderStorageBuffer(const ShaderStorageBuffer&) = delete;
    ShaderStorageBuffer& operator=(const ShaderStorageBuffer&) = delete;
    ShaderStorageBuffer(ShaderStorageBuffer&& other) noexcept;
    ShaderStorageBuffer& operator=(ShaderStorageBuffer&& other) noexcept;

    bool create(size_t size_bytes, const void* data, BufferUsage usage);
    void destroy();
    void bind_base(int binding_point) const;
    void update(size_t offset, size_t size, const void* data);

    /// Read data from the buffer back to CPU memory.
    /// @param offset Byte offset into the buffer
    /// @param size Number of bytes to read
    /// @param dst Destination buffer (must be at least size bytes)
    /// @return true if readback succeeded
    bool readback(size_t offset, size_t size, void* dst) const;

    /// Get native handle (for legacy code that needs direct GL access)
    uint32_t handle() const;
    size_t size() const;
    bool valid() const;

    /// Get the underlying RHI buffer
    rhi::RHIBuffer* rhi_buffer() { return m_buffer.get(); }
    const rhi::RHIBuffer* rhi_buffer() const { return m_buffer.get(); }

private:
    std::unique_ptr<rhi::RHIBuffer> m_buffer;
};

} // namespace engine::graphics
