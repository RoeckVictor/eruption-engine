#pragma once

#include "engine/graphics/Texture.h"
#include <entt/entt.hpp>
#include <string>
#include <unordered_map>
#include <memory>

namespace editor {

class PixelGridTextureCache {
public:
    ~PixelGridTextureCache() = default;

    // Get or load a texture for a pixel grid entity
    void* get(entt::entity entity, const std::string& path);

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
};

}
