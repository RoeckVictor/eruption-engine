#pragma once

#include "RHITypes.h"

namespace engine::rhi {

// Abstract framebuffer (render target)
class RHIFramebuffer {
public:
    virtual ~RHIFramebuffer() = default;

    RHIFramebuffer(const RHIFramebuffer&) = delete;
    RHIFramebuffer& operator=(const RHIFramebuffer&) = delete;

    virtual void bind() = 0;
    virtual void unbind() = 0;
    virtual bool resize(int width, int height) = 0;

    virtual RHITexture* color_attachment(uint32_t index) = 0;
    virtual RHITexture* depth_stencil_attachment() = 0;

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

}
