#pragma once

#include "engine/rhi/RHIFramebuffer.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace engine::rhi {

class GLTexture;

// OpenGL implementation of RHIFramebuffer
class GLFramebuffer : public RHIFramebuffer {
public:
    GLFramebuffer() = default;
    ~GLFramebuffer() override;

    GLFramebuffer(GLFramebuffer&& other) noexcept;
    GLFramebuffer& operator=(GLFramebuffer&& other) noexcept;

    bool init(const FramebufferDesc& desc);

    bool init_with_new_textures(int width, int height, uint32_t color_count,
                                TextureFormat color_format, bool create_depth,
                                TextureFormat depth_format = TextureFormat::Depth24Stencil8);

    void destroy();

    void bind() override;
    void unbind() override;
    bool resize(int width, int height) override;
    RHITexture* color_attachment(uint32_t index) override;
    RHITexture* depth_stencil_attachment() override;
    void* native_handle() const override { return reinterpret_cast<void*>(static_cast<uintptr_t>(m_fbo)); }

    uint32_t handle() const { return m_fbo; }

private:
    uint32_t m_fbo = 0;

    std::vector<std::unique_ptr<GLTexture>> m_owned_color_textures;
    std::unique_ptr<GLTexture> m_owned_depth_texture;

    std::vector<RHITexture*> m_color_attachments;
    RHITexture* m_depth_attachment = nullptr;

    TextureFormat m_color_format = TextureFormat::RGBA8;
    TextureFormat m_depth_format = TextureFormat::Depth24Stencil8;
    bool m_owns_textures = false;
};

}
