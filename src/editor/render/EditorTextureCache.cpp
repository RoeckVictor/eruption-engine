#include "EditorTextureCache.h"
#include <stb_image.h>

namespace editor {

void EditorTextureCache::ensure_white_texture() {
    if (m_white_texture) return;

    m_white_texture = std::make_unique<engine::graphics::Texture>();
    uint8_t white_pixel[4] = { 255, 255, 255, 255 };
    m_white_texture->create_2d(1, 1, engine::graphics::TextureFormat::RGBA8,
                               engine::graphics::TextureFilter::Nearest,
                               engine::graphics::TextureWrap::ClampToEdge,
                               white_pixel);
}

void* EditorTextureCache::white_texture() {
    ensure_white_texture();
    return m_white_texture ? m_white_texture->imgui_texture_id() : nullptr;
}

void* EditorTextureCache::get(const std::string& path, int& out_width, int& out_height) {
    if (path.empty()) {
        ensure_white_texture();
        out_width = 1;
        out_height = 1;
        return m_white_texture ? m_white_texture->imgui_texture_id() : nullptr;
    }

    // Check cache
    auto it = m_cache.find(path);
    if (it != m_cache.end()) {
        out_width = it->second.width;
        out_height = it->second.height;
        return it->second.texture ? it->second.texture->imgui_texture_id() : nullptr;
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
        return m_white_texture ? m_white_texture->imgui_texture_id() : nullptr;
    }

    // Create texture using graphics::Texture (RHI-based)
    auto texture = std::make_unique<engine::graphics::Texture>();
    texture->create_2d(width, height, engine::graphics::TextureFormat::RGBA8,
                       engine::graphics::TextureFilter::Linear,
                       engine::graphics::TextureWrap::ClampToEdge,
                       pixels);

    stbi_image_free(pixels);

    void* handle = texture->imgui_texture_id();
    m_cache[path] = CachedTexture{ std::move(texture), width, height };

    out_width = width;
    out_height = height;
    return handle;
}

void EditorTextureCache::clear() {
    m_cache.clear();
}

void EditorTextureCache::invalidate(const std::string& path) {
    m_cache.erase(path);
}

} // namespace editor
