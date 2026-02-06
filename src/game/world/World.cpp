#include "game/world/World.h"
#include "game/world/MaterialData.h"
#include "game/world/GamePixel.h"
#include "game/GameLog.h"

namespace game {

bool World::init(int width, int height, int max_material_slots) {
    if (!m_grid.init(width, height, GAME_PIXEL_SIZE)) return false;

    // Pack game MaterialProps into engine MaterialSlot format
    engine::simulation::MaterialSlot packed_materials[MAT_COUNT];
    for (int i = 0; i < MAT_COUNT; i++) {
        packed_materials[i] = pack_material(MATERIAL_TABLE[i]);
    }

    // Initialize simulation with custom uniform setup callback
    if (!m_simulation.init(packed_materials, MAT_COUNT,
                           width, height, "shaders/sim_step.comp",
                           [](engine::graphics::Shader& shader) {
                               shader.set_uint("u_mat_water", MAT_WATER);
                               shader.set_uint("u_mat_lava",  MAT_LAVA);
                               shader.set_uint("u_mat_ice",   MAT_ICE);
                               shader.set_uint("u_mat_steam", MAT_STEAM);
                           },
                           max_material_slots))
        return false;

    GAME_LOG("World initialized (%dx%d)", width, height);
    return true;
}

void World::shutdown() {
    m_simulation.shutdown();
    m_grid.shutdown();
}

void World::simulate(engine::graphics::RenderContext& ctx) {
    m_simulation.simulate(m_grid, ctx);
}

} // namespace game
