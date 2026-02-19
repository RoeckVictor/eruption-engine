#include "EditorTextureCache.h"
#include <stb_image.h>
#include <utility>

namespace editor {

EditorTextureCache::~EditorTextureCache() {
    clear();
    if (m_white_texture) {
        glDeleteTextures(1, &m_white_texture);
        m_white_texture = 0;
    }
}

EditorTextureCache::EditorTextureCache(EditorTextureCache&& other) noexcept
    : m_cache(std::move(other.m_cache))
    , m_white_texture(other.m_white_texture)
{
    other.m_white_texture = 0;
}

EditorTextureCache& EditorTextureCache::operator=(EditorTextureCache&& other) noexcept {
    if (this != &other) {
        clear();
        if (m_white_texture) {
            glDeleteTextures(1, &m_white_texture);
        }
        m_cache = std::move(other.m_cache);
        m_white_texture = other.m_white_texture;
        other.m_white_texture = 0;
    }
    return *this;
}

void EditorTextureCache::ensure_white_texture() {
    if (m_white_texture != 0) return;

    uint8_t white_pixel[4] = { 255, 255, 255, 255 };
    glGenTextures(1, &m_white_texture);
    glBindTexture(GL_TEXTURE_2D, m_white_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white_pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint EditorTextureCache::white_texture() {
    ensure_white_texture();
    return m_white_texture;
}

GLuint EditorTextureCache::get(const std::string& path, int& out_width, int& out_height) {
    if (path.empty()) {
        ensure_white_texture();
        out_width = 1;
        out_height = 1;
        return m_white_texture;
    }

    // Check cache
    auto it = m_cache.find(path);
    if (it != m_cache.end()) {
        out_width = it->second.width;
        out_height = it->second.height;
        return it->second.handle;
    }

    // Try to load from disk - first with assets/ prefix
    std::string full_path = "assets/" + path;
    int width, height, channels;
    unsigned char* pixels = stbi_load(full_path.c_str(), &width, &height, &channels, 4);

    // Fallback: try path as-is (might be absolute or already prefixed)
    if (!pixels) {
        pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    }

    if (!pixels) {
        ensure_white_texture();
        out_width = 1;
        out_height = 1;
        return m_white_texture;
    }

    // Create OpenGL texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(pixels);

    m_cache[path] = { texture, width, height };

    out_width = width;
    out_height = height;
    return texture;
}

void EditorTextureCache::clear() {
    for (auto& [path, tex] : m_cache) {
        if (tex.handle) {
            glDeleteTextures(1, &tex.handle);
        }
    }
    m_cache.clear();
}

void EditorTextureCache::invalidate(const std::string& path) {
    auto it = m_cache.find(path);
    if (it != m_cache.end()) {
        if (it->second.handle) {
            glDeleteTextures(1, &it->second.handle);
        }
        m_cache.erase(it);
    }
}

} // namespace editor
