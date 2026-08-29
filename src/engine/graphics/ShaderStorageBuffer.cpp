#include "engine/graphics/ShaderStorageBuffer.h"
#include "engine/rhi/RHI.h"
#include "engine/rhi/RHIDevice.h"
#include "engine/rhi/RHIContext.h"
#include "engine/core/Log.h"

namespace engine::graphics {

namespace {

// Convert graphics BufferUsage to RHI BufferUsage
rhi::BufferUsage to_rhi_usage(BufferUsage usage) {
    switch (usage) {
        case BufferUsage::StaticDraw:  return rhi::BufferUsage::Static;
        case BufferUsage::DynamicDraw: return rhi::BufferUsage::Dynamic;
        case BufferUsage::StreamRead:  return rhi::BufferUsage::Stream;
        default:                       return rhi::BufferUsage::Static;
    }
}

} // anonymous namespace

ShaderStorageBuffer::~ShaderStorageBuffer() {
    destroy();
}

ShaderStorageBuffer::ShaderStorageBuffer(ShaderStorageBuffer&& other) noexcept
    : m_buffer(std::move(other.m_buffer))
{
}

ShaderStorageBuffer& ShaderStorageBuffer::operator=(ShaderStorageBuffer&& other) noexcept {
    if (this != &other) {
        m_buffer = std::move(other.m_buffer);
    }
    return *this;
}

bool ShaderStorageBuffer::create(size_t size_bytes, const void* data, BufferUsage usage) {
    destroy();

    auto* device = rhi::get_current_device();
    if (!device) {
        ENGINE_ERR("No RHI device available for buffer creation");
        return false;
    }

    rhi::BufferDesc desc;
    desc.type = rhi::BufferType::Storage;
    desc.usage = to_rhi_usage(usage);
    desc.size = size_bytes;
    desc.initial_data = data;

    m_buffer = device->create_buffer(desc);
    if (!m_buffer || !m_buffer->valid()) {
        ENGINE_ERR("Failed to create SSBO (%zu bytes)", size_bytes);
        return false;
    }

    return true;
}

void ShaderStorageBuffer::destroy() {
    m_buffer.reset();
}

void ShaderStorageBuffer::bind_base(int binding_point) const {
    if (m_buffer) {
        auto* ctx = rhi::get_current_context();
        if (ctx) {
            ctx->bind_storage_buffer(m_buffer.get(), static_cast<uint32_t>(binding_point));
        }
    }
}

void ShaderStorageBuffer::update(size_t offset, size_t size, const void* data) {
    if (m_buffer && data) {
        m_buffer->update(offset, size, data);
    }
}

bool ShaderStorageBuffer::readback(size_t offset, size_t size, void* dst) const {
    if (!m_buffer || !dst) return false;
    return m_buffer->readback(offset, size, dst);
}

uint32_t ShaderStorageBuffer::handle() const {
    if (!m_buffer) return 0;
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(m_buffer->native_handle()));
}

size_t ShaderStorageBuffer::size() const {
    return m_buffer ? m_buffer->size() : 0;
}

bool ShaderStorageBuffer::valid() const {
    return m_buffer != nullptr && m_buffer->valid();
}

} // namespace engine::graphics
