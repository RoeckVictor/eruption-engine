#include "PixelGridLoaderSystem.h"
#include "engine/core/Engine.h"
#include "engine/core/Logger.h"
#include "engine/asset/PixelGridFile.h"
#include "engine/asset/PxgDataParser.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/physics/Colliders.h"
#include "engine/core/EngineContext.h"
#include "editor/core/EditorComponents.h"

namespace engine {

bool PixelGridLoaderSystem::init(Engine& engine) {
    auto& ctx = engine.app_context<EngineContext>();
    m_registry = &ctx.registry;

    Logger::instance().info("PixelGridLoader", "PixelGridLoaderSystem initialized");
    return true;
}

void PixelGridLoaderSystem::update(Engine& engine, float /*dt*/) {
    // Iterate entities with PixelGridComponent
    auto view = m_registry->view<simulation::PixelGridComponent>();

    for (auto entity : view) {
        // Skip disabled entities
        if (m_registry->all_of<editor::EntityInfo>(entity)) {
            if (!m_registry->get<editor::EntityInfo>(entity).enabled_in_hierarchy) {
                continue;
            }
        }

        auto& grid_comp = view.get<simulation::PixelGridComponent>(entity);

        // Skip if already loaded or disabled
        if (!grid_comp.enabled || grid_comp.loaded) {
            continue;
        }

        // Skip if no path specified
        if (grid_comp.pixel_grid_path.empty()) {
            continue;
        }

        // Load the grid
        load_grid_for_entity(entity);
    }
}

void PixelGridLoaderSystem::load_grid_for_entity(entt::entity entity) {
    auto* grid_comp = m_registry->try_get<simulation::PixelGridComponent>(entity);
    if (!grid_comp) {
        return;
    }

    // Get VFS from engine's asset database
    // Note: We need to pass engine reference through - for now use a global or store reference
    // Since we can't easily get Engine& here, we'll need to store VFS* in init()
    // For now, let's use a simpler approach: load directly from filesystem

    Logger::instance().info("PixelGridLoader", "Loading pixel grid: %s", grid_comp->pixel_grid_path.c_str());

    // Load .pxg file using direct filesystem path (temporary - should use VFS)
    auto loaded_file = asset::pxg_load(grid_comp->pixel_grid_path);

    if (!loaded_file) {
        Logger::instance().error("PixelGridLoader", "Failed to load pixel grid: %s", grid_comp->pixel_grid_path.c_str());
        return;
    }

    // Parse .pxg data into structured channels
    auto parsed = asset::parse_pxg(*loaded_file);

    LoadedPixelGrid loaded_grid;
    loaded_grid.width = parsed.width;
    loaded_grid.height = parsed.height;
    loaded_grid.color_rgba = std::move(parsed.color_rgba);
    loaded_grid.material_ids = std::move(parsed.material_ids);
    loaded_grid.has_color_layer = parsed.has_color_layer;
    loaded_grid.has_material_layer = parsed.has_material_layer;
    loaded_grid.origin_x = parsed.origin_x;
    loaded_grid.origin_y = parsed.origin_y;

    // Update component
    grid_comp->width = parsed.width;
    grid_comp->height = parsed.height;
    grid_comp->origin_x = parsed.origin_x;
    grid_comp->origin_y = parsed.origin_y;
    grid_comp->loaded = true;

    // If entity has a DynamicCollider, mark it for regeneration
    auto* dynamic_collider = m_registry->try_get<physics::DynamicCollider>(entity);
    if (dynamic_collider) {
        dynamic_collider->generated = false;
        Logger::instance().info("PixelGridLoader", "Invalidated DynamicCollider for regeneration");
    }

    Logger::instance().info("PixelGridLoader", "Loaded pixel grid %dx%d (color=%s, material=%s)",
                            parsed.width, parsed.height,
                            parsed.has_color_layer ? "yes" : "no",
                            parsed.has_material_layer ? "yes" : "no");

    // Store in cache
    m_loaded_grids[entity] = std::move(loaded_grid);
}

const LoadedPixelGrid* PixelGridLoaderSystem::get_loaded_grid(entt::entity entity) const {
    auto it = m_loaded_grids.find(entity);
    if (it != m_loaded_grids.end()) {
        return &it->second;
    }
    return nullptr;
}

const simulation::LoadedPixelGridData* PixelGridLoaderSystem::get_loaded_grid_data(entt::entity entity) const {
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
    simulation::LoadedPixelGridData data;
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

} // namespace engine
