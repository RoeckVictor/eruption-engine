#include "EditorPixelGridLoader.h"
#include "EditorComponents.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/physics/Colliders.h"
#include "engine/asset/PixelGridFile.h"
#include "engine/asset/PxgDataParser.h"
#include "engine/core/Logger.h"
#include <entt/entt.hpp>
#include <memory>
#include <queue>
#include <cmath>

namespace editor {

void EditorPixelGridLoader::update(entt::registry* registry) {
    if (!registry) return;

    auto view = registry->view<engine::simulation::PixelGridComponent>();

    for (auto entity : view) {
        auto& grid_comp = view.get<engine::simulation::PixelGridComponent>(entity);

        if (grid_comp.loaded || !grid_comp.enabled) {
            continue;
        }

        if (grid_comp.pixel_grid_path.empty()) {
            continue;
        }

        if (registry->all_of<EntityInfo>(entity)) {
            if (!registry->get<EntityInfo>(entity).enabled_in_hierarchy) {
                continue;
            }
        }

        load_grid_for_entity(registry, entity);
    }
}

void EditorPixelGridLoader::load_grid_for_entity(entt::registry* registry, entt::entity entity) {
    auto* grid_comp = registry->try_get<engine::simulation::PixelGridComponent>(entity);
    if (!grid_comp) {
        return;
    }

    engine::Logger::instance().info("EditorLoader", "Loading pixel grid: %s", grid_comp->pixel_grid_path.c_str());

    auto loaded_file = engine::asset::pxg_load(grid_comp->pixel_grid_path);

    if (!loaded_file) {
        engine::Logger::instance().error("EditorLoader", "Failed to load pixel grid: %s", grid_comp->pixel_grid_path.c_str());
        return;
    }

    auto parsed = engine::asset::parse_pxg(*loaded_file);

    LoadedPixelGrid loaded_grid;
    loaded_grid.width = parsed.width;
    loaded_grid.height = parsed.height;
    loaded_grid.color_rgba = std::move(parsed.color_rgba);
    loaded_grid.material_ids = std::move(parsed.material_ids);
    loaded_grid.has_color_layer = parsed.has_color_layer;
    loaded_grid.has_material_layer = parsed.has_material_layer;
    loaded_grid.origin_x = parsed.origin_x;
    loaded_grid.origin_y = parsed.origin_y;

    grid_comp->width = parsed.width;
    grid_comp->height = parsed.height;
    grid_comp->origin_x = parsed.origin_x;
    grid_comp->origin_y = parsed.origin_y;
    grid_comp->loaded = true;

    // Sync BoxCollider size/offset to match the new grid dimensions
    if (auto* box = registry->try_get<engine::physics::BoxCollider>(entity)) {
        box->width = static_cast<float>(parsed.width);
        box->height = static_cast<float>(parsed.height);
        box->offset_x = (parsed.width * 0.5f) - parsed.origin_x;
        box->offset_y = (parsed.height * 0.5f) - parsed.origin_y;
    }

    // If entity has a DynamicCollider, mark it for regeneration
    if (auto* dynamic_collider = registry->try_get<engine::physics::DynamicCollider>(entity)) {
        dynamic_collider->generated = false;
        engine::Logger::instance().info("EditorLoader", "Invalidated DynamicCollider for regeneration");
    }

    engine::Logger::instance().info("EditorLoader", "Loaded pixel grid %dx%d (color=%s, material=%s)",
                                    parsed.width, parsed.height,
                                    parsed.has_color_layer ? "yes" : "no",
                                    parsed.has_material_layer ? "yes" : "no");

    m_loaded_grids[entity] = std::move(loaded_grid);
}

const LoadedPixelGrid* EditorPixelGridLoader::get_loaded_grid(entt::entity entity) const {
    auto it = m_loaded_grids.find(entity);
    if (it != m_loaded_grids.end()) {
        return &it->second;
    }
    return nullptr;
}

const engine::simulation::LoadedPixelGridData* EditorPixelGridLoader::get_loaded_grid_data(entt::entity entity) const {
    auto it = m_loaded_grids.find(entity);
    if (it == m_loaded_grids.end()) {
        return nullptr;
    }

    // Check cache first
    auto cache_it = m_interface_cache.find(entity);
    if (cache_it != m_interface_cache.end()) {
        return &cache_it->second;
    }

    // Convert and cache
    const auto& src = it->second;
    engine::simulation::LoadedPixelGridData data;
    data.width = src.width;
    data.height = src.height;
    data.color_rgba = src.color_rgba;
    data.material_ids = src.material_ids;
    data.has_color_layer = src.has_color_layer;
    data.has_material_layer = src.has_material_layer;
    data.origin_x = src.origin_x;
    data.origin_y = src.origin_y;

    m_interface_cache[entity] = std::move(data);
    return &m_interface_cache[entity];
}

void EditorPixelGridLoader::clear() {
    m_loaded_grids.clear();
    m_interface_cache.clear();
    m_debug_contours.clear();
    m_dirty_entities.clear();
}

void EditorPixelGridLoader::mark_dirty(entt::entity entity) {
    m_dirty_entities.insert(entity);
}

bool EditorPixelGridLoader::is_dirty(entt::entity entity) const {
    return m_dirty_entities.count(entity) > 0;
}

void EditorPixelGridLoader::clear_dirty(entt::entity entity) {
    m_dirty_entities.erase(entity);
}

const DebugContours* EditorPixelGridLoader::get_debug_contours(entt::entity entity) const {
    auto it = m_debug_contours.find(entity);
    if (it != m_debug_contours.end()) {
        return &it->second;
    }
    return nullptr;
}

void EditorPixelGridLoader::regenerate_debug_contours(entt::entity entity, float simplification) {
    // Clear existing cache entry so it will be regenerated
    m_debug_contours.erase(entity);
    generate_debug_contours(entity, simplification);
}

void EditorPixelGridLoader::generate_debug_contours(entt::entity entity, float simplification) const {
    auto grid_it = m_loaded_grids.find(entity);
    if (grid_it == m_loaded_grids.end()) {
        return;
    }

    const auto& grid = grid_it->second;
    if (grid.width <= 0 || grid.height <= 0) {
        return;
    }

    // Build solid grid from color data (any non-transparent pixel is solid)
    auto solid_grid = std::make_unique<bool[]>(grid.width * grid.height);

    if (grid.has_color_layer && !grid.color_rgba.empty()) {
        for (int i = 0; i < grid.width * grid.height; i++) {
            // Alpha channel is at index i*4 + 3
            solid_grid[i] = (grid.color_rgba[i * 4 + 3] > 0);
        }
    } else if (grid.has_material_layer && !grid.material_ids.empty()) {
        for (int i = 0; i < grid.width * grid.height; i++) {
            solid_grid[i] = (grid.material_ids[i] > 0);
        }
    } else {
        return; // No pixel data available
    }

    // Generate contours using the simplification value
    // Map simplification (0-1) to epsilon (0.5 - 3.0 pixels)
    float epsilon = 0.5f + simplification * 2.5f;
    auto contours = engine::physics::ContourGenerator::generate(solid_grid.get(), grid.width, grid.height, epsilon);

    DebugContours debug;
    debug.contours = std::move(contours);
    debug.origin_x = grid.origin_x;
    debug.origin_y = grid.origin_y;
    debug.height = grid.height;

    m_debug_contours[entity] = std::move(debug);
}

engine::simulation::LoadedPixelGridData* EditorPixelGridLoader::get_mutable_grid_data(entt::entity entity) {
    // Ensure the interface cache is populated
    get_loaded_grid_data(entity);

    auto cache_it = m_interface_cache.find(entity);
    if (cache_it != m_interface_cache.end()) {
        return &cache_it->second;
    }
    return nullptr;
}

bool EditorPixelGridLoader::erase_pixels(entt::entity entity, int center_x, int center_y, int radius) {
    auto grid_it = m_loaded_grids.find(entity);
    if (grid_it == m_loaded_grids.end()) {
        engine::Logger::instance().warning("PixelGridLoader",
            "erase_pixels: entity not found in loaded grids cache");
        return false;
    }

    auto& grid = grid_it->second;
    if (grid.width <= 0 || grid.height <= 0) {
        engine::Logger::instance().warning("PixelGridLoader",
            "erase_pixels: grid has invalid dimensions (%dx%d)", grid.width, grid.height);
        return false;
    }

    bool erased_any = false;
    int r2 = radius * radius;

    // Erase in circular region
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy > r2) continue;

            int px = center_x + dx;
            int py = center_y + dy;

            if (px < 0 || px >= grid.width || py < 0 || py >= grid.height) continue;

            int idx = py * grid.width + px;

            // Check if pixel is non-empty
            bool has_pixel = false;
            if (grid.has_material_layer && !grid.material_ids.empty()) {
                if (grid.material_ids[idx] != 0) {
                    grid.material_ids[idx] = 0;
                    has_pixel = true;
                }
            }
            if (grid.has_color_layer && !grid.color_rgba.empty()) {
                int color_idx = idx * 4;
                if (grid.color_rgba[color_idx + 3] != 0) {
                    grid.color_rgba[color_idx] = 0;
                    grid.color_rgba[color_idx + 1] = 0;
                    grid.color_rgba[color_idx + 2] = 0;
                    grid.color_rgba[color_idx + 3] = 0;
                    has_pixel = true;
                }
            }

            if (has_pixel) {
                erased_any = true;
            }
        }
    }

    if (erased_any) {
        // Invalidate caches - will be lazily regenerated on next access
        m_interface_cache.erase(entity);
        m_debug_contours.erase(entity);

        // Mark entity as dirty for texture regeneration
        m_dirty_entities.insert(entity);
    }

    return erased_any;
}

std::pair<std::vector<int>, int> EditorPixelGridLoader::label_components_internal(
    const std::vector<uint8_t>& materials, int width, int height) const {

    std::vector<int> labels(width * height, -1);
    int component_count = 0;

    auto idx = [width](int x, int y) { return y * width + x; };

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int i = idx(x, y);
            if (materials[i] == 0 || labels[i] >= 0) continue;

            // BFS flood fill
            std::queue<std::pair<int, int>> q;
            q.push({x, y});
            labels[i] = component_count;

            while (!q.empty()) {
                auto [cx, cy] = q.front();
                q.pop();

                // 4-connected neighbors
                constexpr int dx[] = {-1, 1, 0, 0};
                constexpr int dy[] = {0, 0, -1, 1};

                for (int d = 0; d < 4; d++) {
                    int nx = cx + dx[d];
                    int ny = cy + dy[d];

                    if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;

                    int ni = idx(nx, ny);
                    if (materials[ni] != 0 && labels[ni] < 0) {
                        labels[ni] = component_count;
                        q.push({nx, ny});
                    }
                }
            }

            component_count++;
        }
    }

    return {labels, component_count};
}

int EditorPixelGridLoader::count_components(entt::entity entity) const {
    auto grid_it = m_loaded_grids.find(entity);
    if (grid_it == m_loaded_grids.end()) {
        return 0;
    }

    const auto& grid = grid_it->second;
    if (grid.width <= 0 || grid.height <= 0) {
        return 0;
    }

    // Use material_ids if available, otherwise derive from color alpha
    std::vector<uint8_t> materials;
    if (grid.has_material_layer && !grid.material_ids.empty()) {
        materials = grid.material_ids;
    } else if (grid.has_color_layer && !grid.color_rgba.empty()) {
        materials.resize(grid.width * grid.height);
        for (int i = 0; i < grid.width * grid.height; i++) {
            materials[i] = (grid.color_rgba[i * 4 + 3] > 0) ? 1 : 0;
        }
    } else {
        return 0;
    }

    auto [labels, count] = label_components_internal(materials, grid.width, grid.height);
    return count;
}

std::vector<engine::simulation::PixelGridComponent_Fragment>
EditorPixelGridLoader::extract_components(entt::entity entity) const {
    std::vector<engine::simulation::PixelGridComponent_Fragment> fragments;

    auto grid_it = m_loaded_grids.find(entity);
    if (grid_it == m_loaded_grids.end()) {
        return fragments;
    }

    const auto& grid = grid_it->second;
    if (grid.width <= 0 || grid.height <= 0) {
        return fragments;
    }

    // Use material_ids if available, otherwise derive from color alpha
    std::vector<uint8_t> materials;
    if (grid.has_material_layer && !grid.material_ids.empty()) {
        materials = grid.material_ids;
    } else if (grid.has_color_layer && !grid.color_rgba.empty()) {
        materials.resize(grid.width * grid.height);
        for (int i = 0; i < grid.width * grid.height; i++) {
            materials[i] = (grid.color_rgba[i * 4 + 3] > 0) ? 1 : 0;
        }
    } else {
        return fragments;
    }

    auto [labels, component_count] = label_components_internal(materials, grid.width, grid.height);
    if (component_count <= 0) {
        return fragments;
    }

    // For each component, find bounding box and extract pixels
    for (int c = 0; c < component_count; c++) {
        int min_x = grid.width, max_x = -1;
        int min_y = grid.height, max_y = -1;
        int pixel_count = 0;
        float sum_x = 0.0f, sum_y = 0.0f;

        // First pass: find bounding box and center of mass
        for (int y = 0; y < grid.height; y++) {
            for (int x = 0; x < grid.width; x++) {
                int i = y * grid.width + x;
                if (labels[i] != c) continue;

                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;

                sum_x += x;
                sum_y += y;
                pixel_count++;
            }
        }

        if (pixel_count == 0 || max_x < min_x || max_y < min_y) continue;

        engine::simulation::PixelGridComponent_Fragment frag;
        frag.width = max_x - min_x + 1;
        frag.height = max_y - min_y + 1;
        frag.offset_x = min_x;
        frag.offset_y = min_y;
        frag.center_x = sum_x / static_cast<float>(pixel_count);
        frag.center_y = sum_y / static_cast<float>(pixel_count);
        frag.pixel_count = pixel_count;

        // Calculate new origin relative to fragment bounds
        // Original origin was at (grid.origin_x, grid.origin_y) in grid coords
        frag.origin_x = grid.origin_x - min_x;
        frag.origin_y = grid.origin_y - min_y;

        // Extract pixel data
        frag.material_ids.resize(frag.width * frag.height, 0);
        if (grid.has_color_layer && !grid.color_rgba.empty()) {
            frag.color_rgba.resize(frag.width * frag.height * 4, 0);
        }

        for (int y = 0; y < frag.height; y++) {
            for (int x = 0; x < frag.width; x++) {
                int src_x = min_x + x;
                int src_y = min_y + y;
                int src_i = src_y * grid.width + src_x;
                int dst_i = y * frag.width + x;

                if (labels[src_i] != c) continue;

                // Copy material
                if (grid.has_material_layer && !grid.material_ids.empty()) {
                    frag.material_ids[dst_i] = grid.material_ids[src_i];
                } else {
                    frag.material_ids[dst_i] = 1; // Derived from color
                }

                // Copy color
                if (grid.has_color_layer && !grid.color_rgba.empty()) {
                    int src_color = src_i * 4;
                    int dst_color = dst_i * 4;
                    frag.color_rgba[dst_color] = grid.color_rgba[src_color];
                    frag.color_rgba[dst_color + 1] = grid.color_rgba[src_color + 1];
                    frag.color_rgba[dst_color + 2] = grid.color_rgba[src_color + 2];
                    frag.color_rgba[dst_color + 3] = grid.color_rgba[src_color + 3];
                }
            }
        }

        fragments.push_back(std::move(frag));
    }

    // Sort by pixel count (largest first)
    std::sort(fragments.begin(), fragments.end(),
        [](const auto& a, const auto& b) { return a.pixel_count > b.pixel_count; });

    return fragments;
}

void EditorPixelGridLoader::update_grid_data(entt::entity entity, const engine::simulation::LoadedPixelGridData& data) {
    LoadedPixelGrid grid;
    grid.width = data.width;
    grid.height = data.height;
    grid.color_rgba = data.color_rgba;
    grid.material_ids = data.material_ids;
    grid.has_color_layer = data.has_color_layer;
    grid.has_material_layer = data.has_material_layer;
    grid.origin_x = data.origin_x;
    grid.origin_y = data.origin_y;

    m_loaded_grids[entity] = std::move(grid);

    // Update interface cache
    m_interface_cache[entity] = data;

    // Invalidate debug contours
    m_debug_contours.erase(entity);

    // Mark entity as dirty for texture regeneration
    m_dirty_entities.insert(entity);
}

}
