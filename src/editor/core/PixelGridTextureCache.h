#pragma once

#include <glad/gl.h>
#include <entt/entt.hpp>
#include <string>
#include <unordered_map>

namespace editor {

class PixelGridTextureCache {
public:
    ~PixelGridTextureCache();

    GLuint get(entt::entity entity, const std::string& path);

    void cleanup(entt::registry* registry);

    void clear();

private:
    struct Entry {
        GLuint texture_id = 0;
        std::string source_path;
        int width = 0;
        int height = 0;
    };
    std::unordered_map<entt::entity, Entry> m_cache;
};

}
