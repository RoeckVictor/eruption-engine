#pragma once

#include <entt/fwd.hpp>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <string>

namespace editor {

/// Loaded pixel grid data for editor preview.
struct LoadedPixelGrid {
    int width = 0;
    int height = 0;

    /// RGBA color from the color layer (width * height * 4 bytes).
    /// Empty if no color layer was found (legacy file).
    std::vector<uint8_t> color_rgba;

    /// Material IDs from the material layer (width * height bytes).
    /// Empty if no material layer was found.
    std::vector<uint8_t> material_ids;

    bool has_color_layer = false;
    bool has_material_layer = false;

    int origin_x = 0;
    int origin_y = 0;
};

/// Loads .pxg files for PixelGridComponent entities in the editor.
///
/// This is a lightweight loader for editor preview - it doesn't integrate
/// with the physics simulation or game systems, it just loads pixel data
/// for visual display in the viewport.
class EditorPixelGridLoader {
public:
    EditorPixelGridLoader() = default;
    ~EditorPixelGridLoader() = default;

    /// Update - checks for unloaded PixelGridComponents and loads them.
    /// Call this from EditorApplication::on_update().
    void update(entt::registry* registry);

    /// Get loaded pixel grid data for an entity.
    /// Returns nullptr if not loaded.
    const LoadedPixelGrid* get_loaded_grid(entt::entity entity) const;

    /// Clear all cached data (call when closing a scene).
    void clear();

private:
    std::unordered_map<entt::entity, LoadedPixelGrid> m_loaded_grids;

    void load_grid_for_entity(entt::registry* registry, entt::entity entity);
};

} // namespace editor
