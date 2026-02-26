#pragma once

#include "engine/core/System.h"
#include <entt/fwd.hpp>
#include <unordered_map>
#include <vector>

namespace engine {

/// Loaded pixel grid data structure
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
};

/// Loads .pxg files for entities with PixelGridComponent.
///
/// This system:
/// - Loads pixel grid files from the VFS
/// - Caches loaded data for access by other systems
/// - Invalidates DynamicCollider when grids reload
///
/// Execution Order: Runs in update phase (not fixed_update) since file I/O
/// doesn't need fixed timestep.
class PixelGridLoaderSystem : public System {
public:
    const char* name() const override { return "PixelGridLoaderSystem"; }
    bool init(Engine& engine) override;
    void update(Engine& engine, float dt) override;

    /// Get loaded pixel grid data for an entity (or nullptr if not loaded)
    const LoadedPixelGrid* get_loaded_grid(entt::entity entity) const;

private:
    entt::registry* m_registry = nullptr;
    std::unordered_map<entt::entity, LoadedPixelGrid> m_loaded_grids;

    /// Load a .pxg file and store in cache
    void load_grid_for_entity(entt::entity entity);
};

} // namespace engine
