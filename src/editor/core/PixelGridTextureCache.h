#pragma once

#include "engine/graphics/Texture.h"
#include <entt/entt.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <cstdint>
#include <vector>

namespace editor {

class PixelGridTextureCache {
public:
    ~PixelGridTextureCache() = default;

    void* get(entt::entity entity, const std::string& path);

    void* update_from_data(entt::entity entity, int width, int height,
                           const std::vector<uint8_t>& rgba_data);

    void* update_from_materials(entt::entity entity, int width, int height,
                                const std::vector<uint8_t>& material_ids,
                                const std::string& material_set = "default");

    void mark_dirty(entt::entity entity);
    bool is_dirty(entt::entity entity) const;
    void clear_dirty(entt::entity entity);

    void invalidate(entt::entity entity);

    void cleanup(entt::registry* registry);
    void clear();

private:
    struct Entry {
        std::unique_ptr<engine::graphics::Texture> texture;
        std::string source_path;
        int width = 0;
        int height = 0;
    };
    std::unordered_map<entt::entity, Entry> m_cache;
    std::unordered_set<entt::entity> m_dirty_entities;
};

}
