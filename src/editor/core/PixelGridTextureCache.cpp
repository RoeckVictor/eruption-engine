#include "PixelGridTextureCache.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/simulation/MaterialLibrary.h"
#include "engine/render/PixelGridRenderer.h"
#include "engine/asset/PixelGridFile.h"
#include "engine/asset/PxgDataParser.h"
#include <vector>

namespace editor {

void* PixelGridTextureCache::get(entt::entity entity, const std::string& path) {
    if (path.empty()) return nullptr;

    // Check cache - return existing texture if path hasn't changed
    auto it = m_cache.find(entity);
    if (it != m_cache.end()) {
        // If source_path is empty, texture was created from runtime data - keep it
        // (don't reload from file even if entity still has a path)
        if (it->second.source_path.empty()) {
            return it->second.texture ? it->second.texture->imgui_texture_id() : nullptr;
        }
        if (it->second.source_path == path) {
            return it->second.texture ? it->second.texture->imgui_texture_id() : nullptr;
        }
        // Path changed - remove old entry (texture will be destroyed automatically)
        m_cache.erase(it);
    }

    auto pxg_file = engine::asset::pxg_load(path);
    if (!pxg_file) return nullptr;

    auto parsed = engine::asset::parse_pxg(*pxg_file);
    if (parsed.width <= 0 || parsed.height <= 0) return nullptr;

    std::vector<uint8_t> rgba;

    if (parsed.has_color_layer && !parsed.color_rgba.empty()) {
        rgba = std::move(parsed.color_rgba);
    } else {
        auto* lib = engine::simulation::MaterialLibraryRegistry::instance().get_library("default");
        std::vector<uint32_t> palette(256, 0x00000000);
        if (lib) {
            palette = lib->build_color_palette();
        }

        int pixel_count = parsed.width * parsed.height;
        rgba.resize(pixel_count * 4);
        for (int i = 0; i < pixel_count; i++) {
            uint8_t mat_id = parsed.material_ids.empty() ? 0 : parsed.material_ids[i];
            if (mat_id == 0) {
                rgba[i * 4 + 0] = 0;
                rgba[i * 4 + 1] = 0;
                rgba[i * 4 + 2] = 0;
                rgba[i * 4 + 3] = 0;
            } else {
                uint32_t color = palette[mat_id];
                rgba[i * 4 + 0] = (color >> 24) & 0xFF;
                rgba[i * 4 + 1] = (color >> 16) & 0xFF;
                rgba[i * 4 + 2] = (color >> 8)  & 0xFF;
                rgba[i * 4 + 3] = (color >> 0)  & 0xFF;
            }
        }
    }

    // Create texture using graphics::Texture (RHI-based)
    auto texture = std::make_unique<engine::graphics::Texture>();
    texture->create_2d(parsed.width, parsed.height, engine::graphics::TextureFormat::RGBA8,
                       engine::graphics::TextureFilter::Nearest,
                       engine::graphics::TextureWrap::ClampToEdge,
                       rgba.data());

    void* handle = texture->imgui_texture_id();
    m_cache[entity] = Entry{ std::move(texture), path, parsed.width, parsed.height };
    return handle;
}

void* PixelGridTextureCache::update_from_data(entt::entity entity, int width, int height,
                                              const std::vector<uint8_t>& rgba_data) {
    if (width <= 0 || height <= 0 || rgba_data.empty()) return nullptr;
    if (static_cast<int>(rgba_data.size()) < width * height * 4) return nullptr;

    auto it = m_cache.find(entity);
    if (it != m_cache.end()) {
        // Check if we can reuse existing texture (same dimensions)
        if (it->second.width == width && it->second.height == height && it->second.texture) {
            // Update existing texture data using upload_sub_2d
            it->second.texture->upload_sub_2d(0, 0, width, height, rgba_data.data());
            m_dirty_entities.erase(entity);
            return it->second.texture->imgui_texture_id();
        }
        // Different dimensions - need to recreate
        m_cache.erase(it);
    }

    // Create new texture
    auto texture = std::make_unique<engine::graphics::Texture>();
    texture->create_2d(width, height, engine::graphics::TextureFormat::RGBA8,
                       engine::graphics::TextureFilter::Nearest,
                       engine::graphics::TextureWrap::ClampToEdge,
                       rgba_data.data());

    void* handle = texture->imgui_texture_id();
    m_cache[entity] = Entry{ std::move(texture), "", width, height };
    m_dirty_entities.erase(entity);
    return handle;
}

void* PixelGridTextureCache::update_from_materials(entt::entity entity, int width, int height,
                                                   const std::vector<uint8_t>& material_ids,
                                                   const std::string& material_set) {
    if (width <= 0 || height <= 0) return nullptr;

    // Build RGBA from material IDs
    auto* lib = engine::simulation::MaterialLibraryRegistry::instance().get_library(material_set);
    std::vector<uint32_t> palette(256, 0x00000000);
    if (lib) {
        palette = lib->build_color_palette();
    }

    int pixel_count = width * height;
    std::vector<uint8_t> rgba(pixel_count * 4);
    for (int i = 0; i < pixel_count; i++) {
        uint8_t mat_id = (i < static_cast<int>(material_ids.size())) ? material_ids[i] : 0;
        if (mat_id == 0) {
            rgba[i * 4 + 0] = 0;
            rgba[i * 4 + 1] = 0;
            rgba[i * 4 + 2] = 0;
            rgba[i * 4 + 3] = 0;
        } else {
            uint32_t color = palette[mat_id];
            rgba[i * 4 + 0] = (color >> 24) & 0xFF;
            rgba[i * 4 + 1] = (color >> 16) & 0xFF;
            rgba[i * 4 + 2] = (color >> 8)  & 0xFF;
            rgba[i * 4 + 3] = (color >> 0)  & 0xFF;
        }
    }

    return update_from_data(entity, width, height, rgba);
}

void PixelGridTextureCache::mark_dirty(entt::entity entity) {
    m_dirty_entities.insert(entity);
}

bool PixelGridTextureCache::is_dirty(entt::entity entity) const {
    return m_dirty_entities.count(entity) > 0;
}

void PixelGridTextureCache::clear_dirty(entt::entity entity) {
    m_dirty_entities.erase(entity);
}

void PixelGridTextureCache::invalidate(entt::entity entity) {
    m_cache.erase(entity);
    m_dirty_entities.erase(entity);
}

void PixelGridTextureCache::cleanup(entt::registry* registry) {
    if (!registry) return;

    std::vector<entt::entity> to_remove;
    for (auto& [entity, cached] : m_cache) {
        if (!registry->valid(entity) ||
            !registry->all_of<engine::simulation::PixelGridComponent,
                              engine::render::PixelGridRenderer>(entity)) {
            to_remove.push_back(entity);
        }
    }

    for (auto entity : to_remove) {
        m_cache.erase(entity);
        m_dirty_entities.erase(entity);
    }
}

void PixelGridTextureCache::clear() {
    m_cache.clear();
    m_dirty_entities.clear();
}

}
