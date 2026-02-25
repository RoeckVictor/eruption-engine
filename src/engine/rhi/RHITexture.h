#pragma once

#include "RHITypes.h"

namespace engine::rhi {

/// Abstract texture resource
class RHITexture {
public:
    virtual ~RHITexture() = default;

    // Non-copyable
    RHITexture(const RHITexture&) = delete;
    RHITexture& operator=(const RHITexture&) = delete;

    /// Upload data to a region of the texture
    /// @param x X offset in pixels
    /// @param y Y offset in pixels
    /// @param w Width in pixels
    /// @param h Height in pixels
    /// @param data Source data pointer (must match texture format)
    virtual void upload(int x, int y, int w, int h, const void* data) = 0;

    /// Read data back from a region of the texture
    /// @param x X offset in pixels
    /// @param y Y offset in pixels
    /// @param w Width in pixels
    /// @param h Height in pixels
    /// @param dst Destination buffer
    /// @param dst_size Size of destination buffer in bytes
    virtual void readback(int x, int y, int w, int h, void* dst, size_t dst_size) const = 0;

    /// Bind this texture to a texture unit for sampling
    /// @param unit Texture unit index (0-15 typically)
    virtual void bind(uint32_t unit) const = 0;

    /// Bind this texture as an image for compute shader access
    /// @param unit Image unit index
    /// @param access Read/write access mode
    virtual void bind_as_image(uint32_t unit, ImageAccess access) = 0;

    /// Generate mipmaps for this texture
    virtual void generate_mipmaps() = 0;

    /// Get the texture's native handle (backend-specific)
    /// For OpenGL: GLuint texture ID
    /// For Vulkan: VkImage
    virtual void* native_handle() const = 0;

    // Accessors
    int width() const { return m_width; }
    int height() const { return m_height; }
    int depth() const { return m_depth; }
    TextureDimension dimension() const { return m_dimension; }
    TextureFormat format() const { return m_format; }
    bool valid() const { return m_valid; }

protected:
    RHITexture() = default;

    int m_width = 0;
    int m_height = 0;
    int m_depth = 1;
    TextureDimension m_dimension = TextureDimension::Tex2D;
    TextureFormat m_format = TextureFormat::RGBA8;
    bool m_valid = false;
};

} // namespace engine::rhi
