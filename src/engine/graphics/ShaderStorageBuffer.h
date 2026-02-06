#pragma once
#include <cstdint>
#include <cstddef>

namespace engine::graphics {

enum class BufferUsage {
    StaticDraw,
    DynamicDraw,
    StreamRead,  // For readback-friendly buffers
};

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

    uint32_t handle() const { return m_handle; }
    size_t size() const { return m_size; }
    bool valid() const { return m_handle != 0; }

private:
    uint32_t m_handle = 0;
    size_t m_size = 0;
};

} // namespace engine::graphics
