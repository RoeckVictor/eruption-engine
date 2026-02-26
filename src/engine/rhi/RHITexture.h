#pragma once

#include "RHITypes.h"

namespace engine::rhi {

// Abstract texture resource
class RHITexture {
public:
    virtual ~RHITexture() = default;

    RHITexture(const RHITexture&) = delete;
    RHITexture& operator=(const RHITexture&) = delete;

    virtual void upload(int x, int y, int w, int h, const void* data) = 0;
    virtual void readback(int x, int y, int w, int h, void* dst, size_t dst_size) const = 0;

    virtual void bind(uint32_t unit) const = 0;
    virtual void bind_as_image(uint32_t unit, ImageAccess access) = 0;
    virtual void generate_mipmaps() = 0;
    virtual void* native_handle() const = 0;

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

}
