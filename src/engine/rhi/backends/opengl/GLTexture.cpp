#include "GLTexture.h"
#include <glad/gl.h>
#include <cstring>

namespace engine::rhi {

namespace {

GLenum dimension_to_gl_target(TextureDimension dim) {
    switch (dim) {
        case TextureDimension::Tex1D:   return GL_TEXTURE_1D;
        case TextureDimension::Tex2D:   return GL_TEXTURE_2D;
        case TextureDimension::Tex3D:   return GL_TEXTURE_3D;
        case TextureDimension::TexCube: return GL_TEXTURE_CUBE_MAP;
        default: return GL_TEXTURE_2D;
    }
}

GLenum filter_to_gl(TextureFilter filter) {
    switch (filter) {
        case TextureFilter::Nearest: return GL_NEAREST;
        case TextureFilter::Linear:  return GL_LINEAR;
        default: return GL_NEAREST;
    }
}

GLenum wrap_to_gl(TextureWrap wrap) {
    switch (wrap) {
        case TextureWrap::Clamp:         return GL_CLAMP_TO_EDGE;
        case TextureWrap::Repeat:        return GL_REPEAT;
        case TextureWrap::Mirror:        return GL_MIRRORED_REPEAT;
        case TextureWrap::ClampToBorder: return GL_CLAMP_TO_BORDER;
        default: return GL_CLAMP_TO_EDGE;
    }
}

GLenum access_to_gl(ImageAccess access) {
    switch (access) {
        case ImageAccess::ReadOnly:  return GL_READ_ONLY;
        case ImageAccess::WriteOnly: return GL_WRITE_ONLY;
        case ImageAccess::ReadWrite: return GL_READ_WRITE;
        default: return GL_READ_WRITE;
    }
}

struct GLFormatInfo {
    GLenum internal_format;
    GLenum format;
    GLenum type;
};

GLFormatInfo format_to_gl(TextureFormat fmt) {
    switch (fmt) {
        case TextureFormat::R8:             return {GL_R8, GL_RED, GL_UNSIGNED_BYTE};
        case TextureFormat::RG8:            return {GL_RG8, GL_RG, GL_UNSIGNED_BYTE};
        case TextureFormat::RGB8:           return {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE};
        case TextureFormat::RGBA8:          return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
        case TextureFormat::R8UI:           return {GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE};
        case TextureFormat::RG8UI:          return {GL_RG8UI, GL_RG_INTEGER, GL_UNSIGNED_BYTE};
        case TextureFormat::RGB8UI:         return {GL_RGB8UI, GL_RGB_INTEGER, GL_UNSIGNED_BYTE};
        case TextureFormat::RGBA8UI:        return {GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE};
        case TextureFormat::R16F:           return {GL_R16F, GL_RED, GL_HALF_FLOAT};
        case TextureFormat::RG16F:          return {GL_RG16F, GL_RG, GL_HALF_FLOAT};
        case TextureFormat::RGB16F:         return {GL_RGB16F, GL_RGB, GL_HALF_FLOAT};
        case TextureFormat::RGBA16F:        return {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT};
        case TextureFormat::R32F:           return {GL_R32F, GL_RED, GL_FLOAT};
        case TextureFormat::RG32F:          return {GL_RG32F, GL_RG, GL_FLOAT};
        case TextureFormat::RGB32F:         return {GL_RGB32F, GL_RGB, GL_FLOAT};
        case TextureFormat::RGBA32F:        return {GL_RGBA32F, GL_RGBA, GL_FLOAT};
        case TextureFormat::Depth16:        return {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT};
        case TextureFormat::Depth24:        return {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT};
        case TextureFormat::Depth32F:       return {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT};
        case TextureFormat::Depth24Stencil8: return {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8};
        case TextureFormat::Depth32FStencil8: return {GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV};
        default: return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
    }
}

} // anonymous namespace

GLTexture::~GLTexture() {
    destroy();
}

GLTexture::GLTexture(GLTexture&& other) noexcept
    : RHITexture()
{
    m_handle = other.m_handle;
    m_gl_target = other.m_gl_target;
    m_gl_internal_format = other.m_gl_internal_format;
    m_gl_format = other.m_gl_format;
    m_gl_type = other.m_gl_type;
    m_width = other.m_width;
    m_height = other.m_height;
    m_depth = other.m_depth;
    m_dimension = other.m_dimension;
    m_format = other.m_format;
    m_valid = other.m_valid;

    other.m_handle = 0;
    other.m_valid = false;
}

GLTexture& GLTexture::operator=(GLTexture&& other) noexcept {
    if (this != &other) {
        destroy();

        m_handle = other.m_handle;
        m_gl_target = other.m_gl_target;
        m_gl_internal_format = other.m_gl_internal_format;
        m_gl_format = other.m_gl_format;
        m_gl_type = other.m_gl_type;
        m_width = other.m_width;
        m_height = other.m_height;
        m_depth = other.m_depth;
        m_dimension = other.m_dimension;
        m_format = other.m_format;
        m_valid = other.m_valid;

        other.m_handle = 0;
        other.m_valid = false;
    }
    return *this;
}

bool GLTexture::init(const TextureDesc& desc) {
    if (m_handle != 0) {
        destroy();
    }

    m_width = desc.width;
    m_height = desc.height;
    m_depth = desc.depth;
    m_dimension = desc.dimension;
    m_format = desc.format;
    m_gl_target = dimension_to_gl_target(desc.dimension);

    auto fmt_info = format_to_gl(desc.format);
    m_gl_internal_format = fmt_info.internal_format;
    m_gl_format = fmt_info.format;
    m_gl_type = fmt_info.type;

    glGenTextures(1, &m_handle);
    if (m_handle == 0) {
        return false;
    }

    glBindTexture(m_gl_target, m_handle);

    // Set filtering
    glTexParameteri(m_gl_target, GL_TEXTURE_MIN_FILTER, filter_to_gl(desc.min_filter));
    glTexParameteri(m_gl_target, GL_TEXTURE_MAG_FILTER, filter_to_gl(desc.mag_filter));

    // Set wrapping
    glTexParameteri(m_gl_target, GL_TEXTURE_WRAP_S, wrap_to_gl(desc.wrap_u));
    glTexParameteri(m_gl_target, GL_TEXTURE_WRAP_T, wrap_to_gl(desc.wrap_v));
    if (m_dimension == TextureDimension::Tex3D || m_dimension == TextureDimension::TexCube) {
        glTexParameteri(m_gl_target, GL_TEXTURE_WRAP_R, wrap_to_gl(desc.wrap_w));
    }

    // Allocate storage
    switch (m_dimension) {
        case TextureDimension::Tex1D:
            glTexImage1D(m_gl_target, 0, m_gl_internal_format,
                         m_width, 0, m_gl_format, m_gl_type, desc.initial_data);
            break;

        case TextureDimension::Tex2D:
            glTexImage2D(m_gl_target, 0, m_gl_internal_format,
                         m_width, m_height, 0, m_gl_format, m_gl_type, desc.initial_data);
            break;

        case TextureDimension::Tex3D:
            glTexImage3D(m_gl_target, 0, m_gl_internal_format,
                         m_width, m_height, m_depth, 0, m_gl_format, m_gl_type, desc.initial_data);
            break;

        case TextureDimension::TexCube:
            for (int face = 0; face < 6; ++face) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, m_gl_internal_format,
                             m_width, m_height, 0, m_gl_format, m_gl_type, nullptr);
            }
            break;
    }

    if (desc.generate_mipmaps) {
        glGenerateMipmap(m_gl_target);
        glTexParameteri(m_gl_target, GL_TEXTURE_MIN_FILTER,
                        desc.min_filter == TextureFilter::Linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST);
    }

    glBindTexture(m_gl_target, 0);

    m_valid = true;
    return true;
}

void GLTexture::destroy() {
    if (m_handle != 0) {
        glDeleteTextures(1, &m_handle);
        m_handle = 0;
    }
    m_valid = false;
}

void GLTexture::upload(int x, int y, int w, int h, const void* data) {
    if (!m_valid || !data) return;

    glBindTexture(m_gl_target, m_handle);

    switch (m_dimension) {
        case TextureDimension::Tex1D:
            glTexSubImage1D(m_gl_target, 0, x, w, m_gl_format, m_gl_type, data);
            break;

        case TextureDimension::Tex2D:
            glTexSubImage2D(m_gl_target, 0, x, y, w, h, m_gl_format, m_gl_type, data);
            break;

        default:
            // 3D and cubemap uploads would need additional parameters
            break;
    }

    glBindTexture(m_gl_target, 0);
}

void GLTexture::readback(int x, int y, int w, int h, void* dst, size_t dst_size) const {
    if (!m_valid || !dst) return;
    if (m_dimension != TextureDimension::Tex2D) return;  // Only 2D supported for now

    (void)dst_size;  // Could add size validation

    // Create a temporary framebuffer to read from the texture
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_gl_target, m_handle, 0);

    glReadPixels(x, y, w, h, m_gl_format, m_gl_type, dst);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
}

void GLTexture::bind(uint32_t unit) const {
    if (!m_valid) return;

    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(m_gl_target, m_handle);
}

void GLTexture::bind_as_image(uint32_t unit, ImageAccess access) {
    if (!m_valid) return;

    glBindImageTexture(unit, m_handle, 0, GL_FALSE, 0,
                       access_to_gl(access), m_gl_internal_format);
}

void GLTexture::generate_mipmaps() {
    if (!m_valid) return;

    glBindTexture(m_gl_target, m_handle);
    glGenerateMipmap(m_gl_target);
    glBindTexture(m_gl_target, 0);
}

} // namespace engine::rhi
