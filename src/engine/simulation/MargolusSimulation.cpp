#include "engine/simulation/MargolusSimulation.h"
#include "engine/simulation/PixelGrid.h"
#include "engine/graphics/RenderContext.h"
#include "engine/core/Log.h"
#include <glad/gl.h>

namespace engine::simulation {

// Must match sim_step.comp layout(local_size_x, local_size_y)
static constexpr int SIM_WORKGROUP_SIZE = 16;
// Margolus neighborhood: each thread processes a 2x2 block
static constexpr int MARGOLUS_BLOCK_SIZE = 2;
// Each material slot uses 2 uint32s
static constexpr int WORDS_PER_MATERIAL = 2;
// 4 Margolus phases per simulation step (one per 2x2 offset)
static constexpr int MARGOLUS_PHASES = 4;

bool MargolusSimulation::init(const MaterialSlot* slots, int mat_count,
                            int grid_width, int grid_height,
                            const char* shader_path,
                            const UniformSetupCallback& setup_uniforms,
                            int max_material_slots) {
    // Pack material slots into SSBO format.
    //
    // Bit layout per material (2 uint32s):
    //   Word 0:  [7:0]   density
    //            [11:8]  category  (4 bits, masked)
    //            [31:12] user_data[0] low 20 bits
    //   Word 1:  [31:0]  user_data[1] full 32 bits
    //
    // Games pack their own mechanics into user_data.
    // The compute shader interprets the layout.
    std::vector<uint32_t> packed(max_material_slots * WORDS_PER_MATERIAL, 0);

    int count = mat_count < max_material_slots ? mat_count : max_material_slots;
    for (int i = 0; i < count; i++) {
        const MaterialSlot& m = slots[i];
        packed[i * 2 + 0] = (uint32_t)(m.density)
                           | (((uint32_t)m.category & 0xFu) << 8)
                           | ((m.user_data[0] & 0xFFFFFu) << 12);
        packed[i * 2 + 1] = m.user_data[1];
    }

    m_material_ssbo.create(packed.size() * sizeof(uint32_t), packed.data(),
                           graphics::BufferUsage::StaticDraw);

    if (!m_sim_shader.load_compute(shader_path)) {
        ENGINE_ERR("Failed to load simulation compute shader: %s", shader_path);
        return false;
    }

    // Set engine-managed uniforms (grid dimensions are constant)
    m_sim_shader.use();
    m_sim_shader.set_int("u_grid_width", grid_width);
    m_sim_shader.set_int("u_grid_height", grid_height);

    // Allow game to set custom uniforms
    if (setup_uniforms) {
        setup_uniforms(m_sim_shader);
    }

    ENGINE_LOG("Margolus simulation initialized");
    return true;
}

void MargolusSimulation::shutdown() {
    m_material_ssbo.destroy();
    m_sim_shader.destroy();
}

void MargolusSimulation::simulate(PixelGrid& grid, graphics::RenderContext& ctx) {
    m_sim_shader.use();
    m_material_ssbo.bind_base(2);

    // Set pixel size uniform (needed by shader for SSBO access)
    m_sim_shader.set_uint("u_pixel_size", static_cast<uint32_t>(grid.pixel_size()));

    int dispatch_x = grid.width()  / MARGOLUS_BLOCK_SIZE / SIM_WORKGROUP_SIZE;
    int dispatch_y = grid.height() / MARGOLUS_BLOCK_SIZE / SIM_WORKGROUP_SIZE;

    for (int phase = 0; phase < MARGOLUS_PHASES; phase++) {
        // Bind pixel SSBOs instead of textures
        grid.bind_read_ssbo(0);
        grid.bind_write_ssbo(1);

        m_sim_shader.set_int("u_phase", phase);
        m_sim_shader.set_uint("u_frame", grid.frame_counter());

        // Dispatch with SSBO barrier (simulation writes to grid SSBO, next phase reads it)
        ctx.dispatch_compute(dispatch_x, dispatch_y, 1, GL_SHADER_STORAGE_BARRIER_BIT);

        grid.swap();
    }

    grid.increment_frame();
}

} // namespace engine::simulation
