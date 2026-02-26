#pragma once

#include "engine/rhi/RHIBuffer.h"
#include <cstdint>

namespace engine::rhi {

// OpenGL implementation of RHIBuffer
class GLBuffer : public RHIBuffer {
public:
    GLBuffer() = default;
    ~GLBuffer() override;

    GLBuffer(GLBuffer&& other) noexcept;
    GLBuffer& operator=(GLBuffer&& other) noexcept;

    bool init(const BufferDesc& desc);
    void destroy();

    void update(size_t offset, size_t size, const void* data) override;
    bool resize(size_t new_size, const void* new_data = nullptr) override;
    bool readback(size_t offset, size_t size, void* dst) const override;
    void* map_read(size_t offset, size_t size) override;
    void unmap() override;
    void bind(uint32_t slot) override;
    void* native_handle() const override { return reinterpret_cast<void*>(static_cast<uintptr_t>(m_handle)); }

    uint32_t handle() const { return m_handle; }

private:
    uint32_t m_handle = 0;
};

}
