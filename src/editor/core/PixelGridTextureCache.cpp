#include "PixelGridTextureCache.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/simulation/MaterialLibrary.h"
#include "engine/render/PixelGridRenderer.h"
#include "engine/asset/PixelGridFile.h"
#include "engine/asset/PxgDataParser.h"
#include <vector>

namespace editor {

PixelGridTextureCache::~PixelGridTextureCache() {
    clear();
}

GLuint PixelGridTextureCache::get(entt::entity entity, const std::string& path) {
    if (path.empty()) return 0;

    // Check cache - return existing texture if path hasn't changed
    auto it = m_cache.find(entity);
    if (it != m_cache.end()) {
        if (it->second.source_path == path) {
            return it->second.texture_id;
        }
        // Path changed - delete old texture
        if (it->second.texture_id) {
            glDeleteTextures(1, &it->second.texture_id);
        }
        m_cache.erase(it);
    }

    auto pxg_file = engine::asset::pxg_load(path);
    if (!pxg_file) return 0;

    auto parsed = engine::asset::parse_pxg(*pxg_file);
    if (parsed.width <= 0 || parsed.height <= 0) return 0;

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

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, parsed.width, parsed.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_cache[entity] = Entry{tex, path, parsed.width, parsed.height};
    return tex;
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
        auto& cached = m_cache[entity];
        if (cached.texture_id) {
            glDeleteTextures(1, &cached.texture_id);
        }
        m_cache.erase(entity);
    }
}

void PixelGridTextureCache::clear() {
    for (auto& [entity, cached] : m_cache) {
        if (cached.texture_id) {
            glDeleteTextures(1, &cached.texture_id);
        }
    }
    m_cache.clear();
}

}