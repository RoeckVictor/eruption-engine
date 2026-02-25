#pragma once

#include "engine/rhi/RHIFramebuffer.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace engine::rhi {

class GLTexture;

/// OpenGL implementation of RHIFramebuffer
class GLFramebuffer : public RHIFramebuffer {
public:
    GLFramebuffer() = default;
    ~GLFramebuffer() override;

    // Move semantics
    GLFramebuffer(GLFramebuffer&& other) noexcept;
    GLFramebuffer& operator=(GLFramebuffer&& other) noexcept;

    /// Initialize the framebuffer from existing textures
    bool init(const FramebufferDesc& desc);

    /// Initialize the framebuffer by creating new textures
    /// This is a convenience method for creating framebuffers with owned textures
    bool init_with_new_textures(int width, int height, uint32_t color_count,
                                TextureFormat color_format, bool create_depth,
                                TextureFormat depth_format = TextureFormat::Depth24Stencil8);

    /// Destroy the framebuffer
    void destroy();

    // RHIFramebuffer interface
    void bind() override;
    void unbind() override;
    bool resize(int width, int height) override;
    RHITexture* color_attachment(uint32_t index) override;
    RHITexture* depth_stencil_attachment() override;
    void* native_handle() const override { return reinterpret_cast<void*>(static_cast<uintptr_t>(m_fbo)); }

    /// Get the OpenGL FBO handle
    uint32_t handle() const { return m_fbo; }

private:
    uint32_t m_fbo = 0;

    // Owned textures (when created via init_with_new_textures)
    std::vector<std::unique_ptr<GLTexture>> m_owned_color_textures;
    std::unique_ptr<GLTexture> m_owned_depth_texture;

    // Non-owning pointers to attached textures (for color_attachment() etc.)
    std::vector<RHITexture*> m_color_attachments;
    RHITexture* m_depth_attachment = nullptr;

    // Cached format info for resize (only used with owned textures)
    TextureFormat m_color_format = TextureFormat::RGBA8;
    TextureFormat m_depth_format = TextureFormat::Depth24Stencil8;
    bool m_owns_textures = false;
};

} // namespace engine::rhi
