#pragma once

#include "engine/rhi/RHITexture.h"
#include <cstdint>

namespace engine::rhi {

// OpenGL implementation of RHITexture
class GLTexture : public RHITexture {
public:
    GLTexture() = default;
    ~GLTexture() override;

    GLTexture(GLTexture&& other) noexcept;
    GLTexture& operator=(GLTexture&& other) noexcept;

    bool init(const TextureDesc& desc);
    void destroy();

    void upload(int x, int y, int w, int h, const void* data) override;
    void readback(int x, int y, int w, int h, void* dst, size_t dst_size) const override;
    void bind(uint32_t unit) const override;
    void bind_as_image(uint32_t unit, ImageAccess access) override;
    void generate_mipmaps() override;
    void* native_handle() const override { return reinterpret_cast<void*>(static_cast<uintptr_t>(m_handle)); }

    uint32_t handle() const { return m_handle; }
    uint32_t gl_target() const { return m_gl_target; }
    uint32_t gl_format() const { return m_gl_format; }
    uint32_t gl_type() const { return m_gl_type; }

private:
    uint32_t m_handle = 0;
    uint32_t m_gl_target = 0;
    uint32_t m_gl_internal_format = 0;
    uint32_t m_gl_format = 0;
    uint32_t m_gl_type = 0;
};

}
