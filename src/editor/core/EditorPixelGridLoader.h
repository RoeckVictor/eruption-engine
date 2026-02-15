#pragma once

#include <entt/fwd.hpp>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <string>

namespace editor {

struct LoadedPixelGrid {
    int width = 0;
    int height = 0;

    std::vector<uint8_t> color_rgba;

    std::vector<uint8_t> material_ids;

    bool has_color_layer = false;
    bool has_material_layer = false;

    int origin_x = 0;
    int origin_y = 0;
};

class EditorPixelGridLoader {
public:
    EditorPixelGridLoader() = default;
    ~EditorPixelGridLoader() = default;

    void update(entt::registry* registry);

    const LoadedPixelGrid* get_loaded_grid(entt::entity entity) const;

    void clear();

private:
    std::unordered_map<entt::entity, LoadedPixelGrid> m_loaded_grids;

    void load_grid_for_entity(entt::registry* registry, entt::entity entity);
};

}
