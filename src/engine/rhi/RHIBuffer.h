#pragma once

#include "RHITypes.h"

namespace engine::rhi {

// Abstract buffer resource (vertex, index, uniform, storage)
class RHIBuffer {
public:
    virtual ~RHIBuffer() = default;

    RHIBuffer(const RHIBuffer&) = delete;
    RHIBuffer& operator=(const RHIBuffer&) = delete;

    virtual void update(size_t offset, size_t size, const void* data) = 0;
    virtual bool resize(size_t new_size, const void* new_data = nullptr) = 0;
    virtual bool readback(size_t offset, size_t size, void* dst) const = 0;
    virtual void* map_read(size_t offset, size_t size) = 0;
    virtual void unmap() = 0;
    virtual void bind(uint32_t slot) = 0;
    virtual void* native_handle() const = 0;

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

}
