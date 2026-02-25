#pragma once

#include "engine/rhi/RHITexture.h"
#include <cstdint>

namespace engine::rhi {

/// OpenGL implementation of RHITexture
class GLTexture : public RHITexture {
public:
    GLTexture() = default;
    ~GLTexture() override;

    // Move semantics
    GLTexture(GLTexture&& other) noexcept;
    GLTexture& operator=(GLTexture&& other) noexcept;

    /// Initialize the texture
    bool init(const TextureDesc& desc);

    /// Destroy the texture
    void destroy();

    // RHITexture interface
    void upload(int x, int y, int w, int h, const void* data) override;
    void readback(int x, int y, int w, int h, void* dst, size_t dst_size) const override;
    void bind(uint32_t unit) const override;
    void bind_as_image(uint32_t unit, ImageAccess access) override;
    void generate_mipmaps() override;
    void* native_handle() const override { return reinterpret_cast<void*>(static_cast<uintptr_t>(m_handle)); }

    /// Get the OpenGL texture handle
    uint32_t handle() const { return m_handle; }

    /// Get the OpenGL target (GL_TEXTURE_2D, etc.)
    uint32_t gl_target() const { return m_gl_target; }

    /// Get the OpenGL pixel format (GL_RGBA, GL_RGBA_INTEGER, etc.)
    uint32_t gl_format() const { return m_gl_format; }

    /// Get the OpenGL pixel type (GL_UNSIGNED_BYTE, GL_FLOAT, etc.)
    uint32_t gl_type() const { return m_gl_type; }

private:
    uint32_t m_handle = 0;
    uint32_t m_gl_target = 0;
    uint32_t m_gl_internal_format = 0;
    uint32_t m_gl_format = 0;
    uint32_t m_gl_type = 0;
};

} // namespace engine::rhi
