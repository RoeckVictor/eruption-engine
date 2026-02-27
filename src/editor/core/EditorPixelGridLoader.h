#pragma once

#include "engine/simulation/IPixelGridLoader.h"
#include "engine/physics/ContourGenerator.h"
#include <entt/fwd.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <string>

namespace editor {

// Keep the existing struct for internal use
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

struct DebugContours {
    std::vector<engine::physics::Contour> contours;
    int origin_x = 0;
    int origin_y = 0;
    int height = 0;
};

class EditorPixelGridLoader : public engine::simulation::IPixelGridLoader {
public:
    EditorPixelGridLoader() = default;
    ~EditorPixelGridLoader() override = default;

    void update(entt::registry* registry);

    const LoadedPixelGrid* get_loaded_grid(entt::entity entity) const;

    const engine::simulation::LoadedPixelGridData* get_loaded_grid_data(entt::entity entity) const override;
    engine::simulation::LoadedPixelGridData* get_mutable_grid_data(entt::entity entity) override;

    bool erase_pixels(entt::entity entity, int center_x, int center_y, int radius) override;

    int count_components(entt::entity entity) const override;
    std::vector<engine::simulation::PixelGridComponent_Fragment> extract_components(entt::entity entity) const override;

    void update_grid_data(entt::entity entity, const engine::simulation::LoadedPixelGridData& data) override;
    const DebugContours* get_debug_contours(entt::entity entity) const;
    void regenerate_debug_contours(entt::entity entity, float simplification);

    void mark_dirty(entt::entity entity);
    bool is_dirty(entt::entity entity) const;
    void clear_dirty(entt::entity entity);

    void clear();

private:
    std::unordered_map<entt::entity, LoadedPixelGrid> m_loaded_grids;

    mutable std::unordered_map<entt::entity, engine::simulation::LoadedPixelGridData> m_interface_cache;

    mutable std::unordered_map<entt::entity, DebugContours> m_debug_contours;

    std::unordered_set<entt::entity> m_dirty_entities;

    void load_grid_for_entity(entt::registry* registry, entt::entity entity);
    void generate_debug_contours(entt::entity entity, float simplification) const;

    std::pair<std::vector<int>, int> label_components_internal(
        const std::vector<uint8_t>& materials, int width, int height) const;
};

}
