#pragma once

#include "RHITypes.h"

namespace engine::rhi {

/// Abstract framebuffer (render target)
class RHIFramebuffer {
public:
    virtual ~RHIFramebuffer() = default;

    // Non-copyable
    RHIFramebuffer(const RHIFramebuffer&) = delete;
    RHIFramebuffer& operator=(const RHIFramebuffer&) = delete;

    /// Bind this framebuffer as the current render target
    virtual void bind() = 0;

    /// Unbind and restore the default framebuffer
    virtual void unbind() = 0;

    /// Resize the framebuffer (recreates attachments)
    /// @param width New width in pixels
    /// @param height New height in pixels
    /// @return true if resize succeeded
    virtual bool resize(int width, int height) = 0;

    /// Get a color attachment texture
    /// @param index Attachment index (0 for first color attachment)
    /// @return The texture, or nullptr if index out of range
    virtual RHITexture* color_attachment(uint32_t index) = 0;

    /// Get the depth/stencil attachment texture
    /// @return The texture, or nullptr if no depth attachment
    virtual RHITexture* depth_stencil_attachment() = 0;

    /// Get the framebuffer's native handle (backend-specific)
    /// For OpenGL: GLuint framebuffer ID
    /// For Vulkan: VkFramebuffer
    virtual void* native_handle() const = 0;

    int width() const { return m_width; }
    int height() const { return m_height; }
    uint32_t color_attachment_count() const { return m_color_count; }
    bool has_depth() const { return m_has_depth; }
    bool valid() const { return m_valid; }

protected:
    RHIFramebuffer() = default;

    int m_width = 0;
    int m_height = 0;
    uint32_t m_color_count = 0;
    bool m_has_depth = false;
    bool m_valid = false;
};

} // namespace engine::rhi
