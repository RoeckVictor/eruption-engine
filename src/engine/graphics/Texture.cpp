#include "engine/graphics/Texture.h"
#include "engine/graphics/GLFormatInfo.h"
#include "engine/core/Log.h"
#include <glad/gl.h>

namespace engine::graphics {

using detail::GLFormatInfo;
using detail::gl_format;

namespace {

GLenum gl_filter(TextureFilter f) {
    switch (f) {
        case TextureFilter::Nearest: return GL_NEAREST;
        case TextureFilter::Linear:  return GL_LINEAR;
    }
    return GL_NEAREST;
}

GLenum gl_wrap(TextureWrap w) {
    switch (w) {
        case TextureWrap::ClampToEdge: return GL_CLAMP_TO_EDGE;
        case TextureWrap::Repeat:      return GL_REPEAT;
    }
    return GL_CLAMP_TO_EDGE;
}

GLenum gl_image_format(TextureFormat fmt) {
    switch (fmt) {
        case TextureFormat::RGBA8:   return GL_RGBA8;
        case TextureFormat::RGBA8UI: return GL_RGBA8UI;
    }
    return GL_RGBA8;
}

} // anonymous namespace

Texture::~Texture() {
    destroy();
}

Texture::Texture(Texture&& other) noexcept
    : m_handle(other.m_handle)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_format(other.m_format)
    , m_is_1d(other.m_is_1d)
{
    other.m_handle = 0;
    other.m_width = 0;
    other.m_height = 0;
    other.m_is_1d = false;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        destroy();
        m_handle = other.m_handle;
        m_width = other.m_width;
        m_height = other.m_height;
        m_format = other.m_format;
        m_is_1d = other.m_is_1d;
        other.m_handle = 0;
        other.m_width = 0;
        other.m_height = 0;
        other.m_is_1d = false;
    }
    return *this;
}

bool Texture::create_2d(int width, int height, TextureFormat format,
                        TextureFilter filter, TextureWrap wrap,
                        const void* initial_data)
{
    if (width <= 0 || height <= 0) {
        ENGINE_ERR("Invalid texture dimensions: %dx%d", width, height);
        return false;
    }

    destroy();

    GLFormatInfo fi = gl_format(format);
    GLenum min_mag = gl_filter(filter);
    GLenum wrap_mode = gl_wrap(wrap);

    glGenTextures(1, &m_handle);
    glBindTexture(GL_TEXTURE_2D, m_handle);
    glTexImage2D(GL_TEXTURE_2D, 0, fi.internal_format,
                 width, height, 0,
                 fi.pixel_format, fi.pixel_type, initial_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_mag);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, min_mag);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        ENGINE_ERR("GL error 0x%X creating 2D texture (%dx%d)", err, width, height);
        destroy();
        return false;
    }

    m_width = width;
    m_height = height;
    m_format = format;
    m_is_1d = false;

    return true;
}

bool Texture::create_1d(int width, TextureFormat format,
                        TextureFilter filter, TextureWrap wrap,
                        const void* initial_data)
{
    if (width <= 0) {
        ENGINE_ERR("Invalid 1D texture width: %d", width);
        return false;
    }

    destroy();

    GLFormatInfo fi = gl_format(format);
    GLenum min_mag = gl_filter(filter);
    GLenum wrap_mode = gl_wrap(wrap);

    glGenTextures(1, &m_handle);
    glBindTexture(GL_TEXTURE_1D, m_handle);
    glTexImage1D(GL_TEXTURE_1D, 0, fi.internal_format,
                 width, 0,
                 fi.pixel_format, fi.pixel_type, initial_data);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, min_mag);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, min_mag);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, wrap_mode);
    glBindTexture(GL_TEXTURE_1D, 0);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        ENGINE_ERR("GL error 0x%X creating 1D texture (width=%d)", err, width);
        destroy();
        return false;
    }

    m_width = width;
    m_height = 1;
    m_format = format;
    m_is_1d = true;

    return true;
}

void Texture::destroy() {
    if (m_handle) {
        glDeleteTextures(1, &m_handle);
        m_handle = 0;
        m_width = 0;
        m_height = 0;
    }
}

void Texture::upload_sub_2d(int x, int y, int w, int h, const void* data) {
    GLFormatInfo fi = gl_format(m_format);
    glBindTexture(GL_TEXTURE_2D, m_handle);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h,
                    fi.pixel_format, fi.pixel_type, data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::readback_sub_2d(int x, int y, int w, int h, void* dst, int dst_size) const {
    GLFormatInfo fi = gl_format(m_format);
    glGetTextureSubImage(m_handle, 0,
                         x, y, 0,
                         w, h, 1,
                         fi.pixel_format, fi.pixel_type,
                         dst_size, dst);
}

void Texture::bind(int unit) const {
    GLenum target = m_is_1d ? GL_TEXTURE_1D : GL_TEXTURE_2D;
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(target, m_handle);
}

void Texture::bind_as_image(int unit, ImageAccess access) const {
    GLenum gl_access;
    switch (access) {
        case ImageAccess::ReadOnly:  gl_access = GL_READ_ONLY;  break;
        case ImageAccess::WriteOnly: gl_access = GL_WRITE_ONLY; break;
        case ImageAccess::ReadWrite: gl_access = GL_READ_WRITE; break;
        default:                     gl_access = GL_READ_ONLY;  break;
    }
    glBindImageTexture(unit, m_handle, 0, GL_FALSE, 0,
                       gl_access, gl_image_format(m_format));
}

} // namespace engine::graphics
