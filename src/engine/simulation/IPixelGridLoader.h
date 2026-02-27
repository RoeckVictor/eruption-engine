#pragma once

#include <entt/fwd.hpp>
#include <cstdint>
#include <vector>

namespace engine::simulation {

/// Loaded pixel grid data (common interface between editor and engine loaders)
struct LoadedPixelGridData {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> color_rgba;
    std::vector<uint8_t> material_ids;
    bool has_color_layer = false;
    bool has_material_layer = false;
    int origin_x = 0;
    int origin_y = 0;
};

/// A connected component extracted from a pixel grid
struct PixelGridComponent_Fragment {
    std::vector<uint8_t> material_ids;
    std::vector<uint8_t> color_rgba;
    int width = 0;
    int height = 0;
    int offset_x = 0;  // Offset in original grid coords
    int offset_y = 0;
    int origin_x = 0;  // New origin relative to fragment bounds
    int origin_y = 0;
    float center_x = 0.0f;  // Center of mass in original grid coords
    float center_y = 0.0f;
    int pixel_count = 0;
};

/// Abstract interface for pixel grid loaders.
/// Both EditorPixelGridLoader and PixelGridLoaderSystem implement this.
class IPixelGridLoader {
public:
    virtual ~IPixelGridLoader() = default;
    virtual const LoadedPixelGridData* get_loaded_grid_data(entt::entity entity) const = 0;

    /// Get mutable pixel data (for runtime modification). Returns nullptr if not supported.
    virtual LoadedPixelGridData* get_mutable_grid_data(entt::entity entity) { return nullptr; }

    /// Erase pixels in a circular region. Returns true if any pixels were erased.
    /// @param entity The entity whose pixel grid to modify
    /// @param center_x Center X in grid coordinates
    /// @param center_y Center Y in grid coordinates
    /// @param radius Radius in pixels
    virtual bool erase_pixels(entt::entity entity, int center_x, int center_y, int radius) { return false; }

    /// Count connected components in the pixel grid.
    /// @return Number of disconnected regions (1 = single connected piece)
    virtual int count_components(entt::entity entity) const { return 1; }

    /// Extract connected components from the pixel grid.
    /// Returns fragments for each disconnected region.
    virtual std::vector<PixelGridComponent_Fragment> extract_components(entt::entity entity) const { return {}; }

    /// Update pixel data for an entity (used after splitting)
    virtual void update_grid_data(entt::entity entity, const LoadedPixelGridData& data) {}
};

} // namespace engine::simulation
