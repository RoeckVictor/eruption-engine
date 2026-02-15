#include "EditorPixelGridLoader.h"
#include "EditorComponents.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/physics/Colliders.h"
#include "engine/asset/PixelGridFile.h"
#include "engine/asset/PxgDataParser.h"
#include "engine/core/Logger.h"
#include <entt/entt.hpp>

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

void EditorPixelGridLoader::clear() {
    m_loaded_grids.clear();
}

}
