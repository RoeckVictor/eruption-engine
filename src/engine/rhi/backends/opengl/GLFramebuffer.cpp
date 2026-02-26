#include "GLFramebuffer.h"
#include "GLTexture.h"
#include <glad/gl.h>

namespace engine::rhi {

GLFramebuffer::~GLFramebuffer() {
    destroy();
}

GLFramebuffer::GLFramebuffer(GLFramebuffer&& other) noexcept
    : m_fbo(other.m_fbo)
    , m_owned_color_textures(std::move(other.m_owned_color_textures))
    , m_owned_depth_texture(std::move(other.m_owned_depth_texture))
    , m_color_attachments(std::move(other.m_color_attachments))
    , m_depth_attachment(other.m_depth_attachment)
    , m_color_format(other.m_color_format)
    , m_depth_format(other.m_depth_format)
    , m_owns_textures(other.m_owns_textures)
{
    m_width = other.m_width;
    m_height = other.m_height;
    m_color_count = other.m_color_count;
    m_has_depth = other.m_has_depth;
    m_valid = other.m_valid;

    other.m_fbo = 0;
    other.m_depth_attachment = nullptr;
    other.m_valid = false;
}

GLFramebuffer& GLFramebuffer::operator=(GLFramebuffer&& other) noexcept {
    if (this != &other) {
        destroy();
        m_fbo = other.m_fbo;
        m_owned_color_textures = std::move(other.m_owned_color_textures);
        m_owned_depth_texture = std::move(other.m_owned_depth_texture);
        m_color_attachments = std::move(other.m_color_attachments);
        m_depth_attachment = other.m_depth_attachment;
        m_color_format = other.m_color_format;
        m_depth_format = other.m_depth_format;
        m_owns_textures = other.m_owns_textures;
        m_width = other.m_width;
        m_height = other.m_height;
        m_color_count = other.m_color_count;
        m_has_depth = other.m_has_depth;
        m_valid = other.m_valid;

        other.m_fbo = 0;
        other.m_depth_attachment = nullptr;
        other.m_valid = false;
    }
    return *this;
}

bool GLFramebuffer::init(const FramebufferDesc& desc) {
    m_width = desc.width;
    m_height = desc.height;
    m_owns_textures = false;

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // Attach color textures
    m_color_attachments.clear();
    for (uint32_t i = 0; i < desc.color_attachment_count; ++i) {
        if (desc.color_attachments[i].texture) {
            auto* gl_tex = static_cast<GLTexture*>(desc.color_attachments[i].texture);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                                   GL_TEXTURE_2D, gl_tex->handle(),
                                   desc.color_attachments[i].mip_level);
            m_color_attachments.push_back(desc.color_attachments[i].texture);
        }
    }
    m_color_count = static_cast<uint32_t>(m_color_attachments.size());

    // Set up draw buffers
    if (!m_color_attachments.empty()) {
        std::vector<GLenum> draw_buffers;
        for (uint32_t i = 0; i < m_color_attachments.size(); ++i) {
            draw_buffers.push_back(GL_COLOR_ATTACHMENT0 + i);
        }
        glDrawBuffers(static_cast<GLsizei>(draw_buffers.size()), draw_buffers.data());
    } else {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }

    // Attach depth/stencil texture
    m_depth_attachment = nullptr;
    m_has_depth = false;
    if (desc.depth_stencil_attachment.texture) {
        auto* gl_tex = static_cast<GLTexture*>(desc.depth_stencil_attachment.texture);
        TextureFormat fmt = gl_tex->format();

        GLenum attachment = GL_DEPTH_STENCIL_ATTACHMENT;
        if (fmt == TextureFormat::Depth16 ||
            fmt == TextureFormat::Depth24 ||
            fmt == TextureFormat::Depth32F) {
            attachment = GL_DEPTH_ATTACHMENT;
        }

        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                               GL_TEXTURE_2D, gl_tex->handle(),
                               desc.depth_stencil_attachment.mip_level);
        m_depth_attachment = desc.depth_stencil_attachment.texture;
        m_has_depth = true;
    }

    // Check completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_valid = (status == GL_FRAMEBUFFER_COMPLETE);
    if (!m_valid) {
        destroy();
        return false;
    }

    return true;
}

bool GLFramebuffer::init_with_new_textures(int width, int height, uint32_t color_count,
                                           TextureFormat color_format, bool create_depth,
                                           TextureFormat depth_format) {
    m_width = width;
    m_height = height;
    m_color_format = color_format;
    m_depth_format = depth_format;
    m_owns_textures = true;

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // Create and attach color textures
    m_owned_color_textures.clear();
    m_color_attachments.clear();
    for (uint32_t i = 0; i < color_count; ++i) {
        auto texture = std::make_unique<GLTexture>();
        TextureDesc tex_desc{};
        tex_desc.width = width;
        tex_desc.height = height;
        tex_desc.format = color_format;

        if (!texture->init(tex_desc)) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            destroy();
            return false;
        }

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                               GL_TEXTURE_2D, texture->handle(), 0);
        m_color_attachments.push_back(texture.get());
        m_owned_color_textures.push_back(std::move(texture));
    }
    m_color_count = color_count;

    // Set up draw buffers
    if (!m_color_attachments.empty()) {
        std::vector<GLenum> draw_buffers;
        for (uint32_t i = 0; i < m_color_attachments.size(); ++i) {
            draw_buffers.push_back(GL_COLOR_ATTACHMENT0 + i);
        }
        glDrawBuffers(static_cast<GLsizei>(draw_buffers.size()), draw_buffers.data());
    }

    // Create and attach depth/stencil texture if requested
    m_depth_attachment = nullptr;
    m_has_depth = false;
    if (create_depth) {
        auto depth_tex = std::make_unique<GLTexture>();
        TextureDesc depth_desc{};
        depth_desc.width = width;
        depth_desc.height = height;
        depth_desc.format = depth_format;

        if (!depth_tex->init(depth_desc)) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            destroy();
            return false;
        }

        GLenum attachment = GL_DEPTH_STENCIL_ATTACHMENT;
        if (depth_format == TextureFormat::Depth16 ||
            depth_format == TextureFormat::Depth24 ||
            depth_format == TextureFormat::Depth32F) {
            attachment = GL_DEPTH_ATTACHMENT;
        }

        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                               GL_TEXTURE_2D, depth_tex->handle(), 0);
        m_depth_attachment = depth_tex.get();
        m_owned_depth_texture = std::move(depth_tex);
        m_has_depth = true;
    }

    // Check completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_valid = (status == GL_FRAMEBUFFER_COMPLETE);
    if (!m_valid) {
        destroy();
        return false;
    }

    return true;
}

void GLFramebuffer::destroy() {
    m_owned_color_textures.clear();
    m_owned_depth_texture.reset();
    m_color_attachments.clear();
    m_depth_attachment = nullptr;

    if (m_fbo != 0) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }

    m_valid = false;
}

void GLFramebuffer::bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
}

void GLFramebuffer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool GLFramebuffer::resize(int width, int height) {
    if (width == m_width && height == m_height) {
        return true;
    }

    // Can only resize framebuffers that own their textures
    if (!m_owns_textures) {
        return false;
    }

    // Store current configuration
    uint32_t color_count = m_color_count;
    bool has_depth = m_has_depth;

    // Recreate framebuffer with new size
    destroy();

    return init_with_new_textures(width, height, color_count, m_color_format,
                                  has_depth, m_depth_format);
}

RHITexture* GLFramebuffer::color_attachment(uint32_t index) {
    if (index < m_color_attachments.size()) {
        return m_color_attachments[index];
    }
    return nullptr;
}

RHITexture* GLFramebuffer::depth_stencil_attachment() {
    return m_depth_attachment;
}

}
