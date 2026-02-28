#include "engine/simulation/MargolusSimulation.h"
#include "engine/simulation/PixelGrid.h"
#include "engine/graphics/RenderContext.h"
#include "engine/rhi/RHITypes.h"
#include "engine/profiler/Profiler.h"
#include "engine/core/Log.h"
#include <cstring>

namespace engine::simulation {

// Must match sim_step.comp layout(local_size_x, local_size_y)
static constexpr int SIM_WORKGROUP_SIZE = 16;
// Margolus neighborhood: each thread processes a 2x2 block
static constexpr int MARGOLUS_BLOCK_SIZE = 2;
// Each material slot uses 2 uint32s
static constexpr int WORDS_PER_MATERIAL = 2;
// 4 Margolus phases per simulation step (one per 2x2 offset)
static constexpr int MARGOLUS_PHASES = 4;

bool MargolusSimulation::init(const std::vector<uint32_t>& material_table,
                                  const std::vector<uint32_t>& interaction_table,
                                  const std::vector<uint32_t>& color_palette,
                                  const std::vector<uint32_t>& category_table,
                                  int grid_width, int grid_height,
                                  const char* shader_path,
                                  const UniformSetupCallback& setup_uniforms,
                                  int chunk_size_x, int chunk_size_y) {
    m_grid_width = grid_width;
    m_grid_height = grid_height;
    m_chunk_size_x = chunk_size_x;
    m_chunk_size_y = chunk_size_y;
    m_num_chunks_x = (grid_width + chunk_size_x - 1) / chunk_size_x;
    m_num_chunks_y = (grid_height + chunk_size_y - 1) / chunk_size_y;

    // Upload material table (already in correct packed format)
    if (material_table.empty()) {
        ENGINE_ERR("MargolusSimulation: Empty material table");
        return false;
    }
    m_material_ssbo.create(material_table.size() * sizeof(uint32_t), material_table.data(),
                           graphics::BufferUsage::StaticDraw);

    // Upload interaction table if present
    if (!interaction_table.empty()) {
        m_interaction_ssbo.create(interaction_table.size() * sizeof(uint32_t),
                                  interaction_table.data(),
                                  graphics::BufferUsage::StaticDraw);
        m_has_interactions = true;
        ENGINE_LOG("MargolusSimulation: Loaded %zu interaction entries",
                   interaction_table.size() / 6);  // 6 uint32s per interaction
    } else {
        m_has_interactions = false;
    }

    // Upload color palette (needed for Blend color behavior)
    if (!color_palette.empty()) {
        m_palette_ssbo.create(color_palette.size() * sizeof(uint32_t),
                              color_palette.data(),
                              graphics::BufferUsage::StaticDraw);
    } else {
        // Create default white palette if none provided
        std::vector<uint32_t> default_palette(256, 0xFFFFFFFF);
        m_palette_ssbo.create(default_palette.size() * sizeof(uint32_t),
                              default_palette.data(),
                              graphics::BufferUsage::StaticDraw);
    }

    // Upload category table (16 categories * 10 words each = 160 words)
    if (!category_table.empty()) {
        m_category_ssbo.create(category_table.size() * sizeof(uint32_t),
                               category_table.data(),
                               graphics::BufferUsage::StaticDraw);
        ENGINE_LOG("MargolusSimulation: Loaded %zu category entries",
                   category_table.size() / 10);  // 10 uint32s per category
    } else {
        // Create default category table with hardcoded EMPTY, STATIC, POWDER, LIQUID, GAS
        std::vector<uint32_t> default_categories(16 * 10, 0);
        m_category_ssbo.create(default_categories.size() * sizeof(uint32_t),
                               default_categories.data(),
                               graphics::BufferUsage::StaticDraw);
    }

    // Create dirty chunks SSBO
    int total_chunks = m_num_chunks_x * m_num_chunks_y;
    std::vector<uint32_t> dirty_init(total_chunks, 0);
    m_dirty_chunks_ssbo.create(total_chunks * sizeof(uint32_t), dirty_init.data(),
                                graphics::BufferUsage::StreamRead);

    if (!m_sim_shader.load_compute(shader_path)) {
        ENGINE_ERR("MargolusSimulation: Failed to load compute shader '%s'", shader_path);
        return false;
    }

    // Set engine-managed uniforms
    m_sim_shader.use();
    m_sim_shader.set_int("u_grid_width", grid_width);
    m_sim_shader.set_int("u_grid_height", grid_height);
    m_sim_shader.set_int("u_chunk_size_x", chunk_size_x);
    m_sim_shader.set_int("u_chunk_size_y", chunk_size_y);
    m_sim_shader.set_int("u_num_chunks_x", m_num_chunks_x);

    // Allow game to set custom uniforms
    if (setup_uniforms) {
        setup_uniforms(m_sim_shader);
    }

    ENGINE_LOG("Margolus simulation (v2) initialized: %dx%d grid, %d chunks, interactions=%s",
               grid_width, grid_height, total_chunks, m_has_interactions ? "yes" : "no");
    return true;
}

void MargolusSimulation::shutdown() {
    m_material_ssbo.destroy();
    m_dirty_chunks_ssbo.destroy();
    m_palette_ssbo.destroy();
    m_category_ssbo.destroy();
    if (m_has_interactions) {
        m_interaction_ssbo.destroy();
    }
    m_sim_shader.destroy();
    m_has_interactions = false;
}

void MargolusSimulation::simulate(PixelGrid& grid, graphics::RenderContext& ctx) {
    PROFILE_SCOPE("MargolusSimulation::simulate");
    m_sim_shader.use();
    m_material_ssbo.bind_base(2);
    m_dirty_chunks_ssbo.bind_base(3);  // Dirty chunk flags for terrain collider optimization

    // Bind interaction table if present (binding 4)
    if (m_has_interactions) {
        m_interaction_ssbo.bind_base(4);
    }

    // Bind color palette (binding 5) - needed for Blend color behavior
    m_palette_ssbo.bind_base(5);

    // Bind category table (binding 6) - needed for data-driven movement
    m_category_ssbo.bind_base(6);

    // Set pixel size uniform (needed by shader for SSBO access)
    m_sim_shader.set_uint("u_pixel_size", static_cast<uint32_t>(grid.pixel_size()));

    // Ceiling division so small grids (< 32x32) still get at least 1 workgroup
    int blocks_x = (grid.width()  + MARGOLUS_BLOCK_SIZE - 1) / MARGOLUS_BLOCK_SIZE;
    int blocks_y = (grid.height() + MARGOLUS_BLOCK_SIZE - 1) / MARGOLUS_BLOCK_SIZE;
    int dispatch_x = (blocks_x + SIM_WORKGROUP_SIZE - 1) / SIM_WORKGROUP_SIZE;
    int dispatch_y = (blocks_y + SIM_WORKGROUP_SIZE - 1) / SIM_WORKGROUP_SIZE;

    for (int phase = 0; phase < MARGOLUS_PHASES; phase++) {
        // Bind pixel SSBOs instead of textures
        grid.bind_read_ssbo(0);
        grid.bind_write_ssbo(1);

        m_sim_shader.set_int("u_phase", phase);
        m_sim_shader.set_uint("u_frame", grid.frame_counter());

        // Dispatch with SSBO barrier (simulation writes to grid SSBO, next phase reads it)
        ctx.dispatch_compute(dispatch_x, dispatch_y, 1, rhi::BarrierFlags::StorageBuffer);

        grid.swap();
    }

    grid.increment_frame();
}

void MargolusSimulation::update_tables(const std::vector<uint32_t>& material_table,
                                        const std::vector<uint32_t>& interaction_table,
                                        const std::vector<uint32_t>& color_palette,
                                        const std::vector<uint32_t>& category_table) {
    // Update material table SSBO
    if (!material_table.empty()) {
        size_t new_size = material_table.size() * sizeof(uint32_t);
        if (new_size > m_material_ssbo.size()) {
            // Buffer needs to grow - recreate it
            m_material_ssbo.destroy();
            m_material_ssbo.create(new_size, material_table.data(), graphics::BufferUsage::StaticDraw);
        } else {
            m_material_ssbo.update(0, new_size, material_table.data());
        }
    }

    // Update interaction table SSBO
    if (!interaction_table.empty()) {
        size_t new_size = interaction_table.size() * sizeof(uint32_t);
        if (!m_has_interactions) {
            // First time creating interaction SSBO
            m_interaction_ssbo.create(new_size, interaction_table.data(),
                                      graphics::BufferUsage::StaticDraw);
            m_has_interactions = true;
        } else if (new_size > m_interaction_ssbo.size()) {
            // Buffer needs to grow - recreate it
            m_interaction_ssbo.destroy();
            m_interaction_ssbo.create(new_size, interaction_table.data(),
                                      graphics::BufferUsage::StaticDraw);
        } else {
            m_interaction_ssbo.update(0, new_size, interaction_table.data());
        }
        ENGINE_LOG("MargolusSimulation: Updated %zu interaction entries",
                   interaction_table.size() / 6);
    }

    // Update color palette SSBO
    if (!color_palette.empty()) {
        size_t new_size = color_palette.size() * sizeof(uint32_t);
        if (new_size > m_palette_ssbo.size()) {
            m_palette_ssbo.destroy();
            m_palette_ssbo.create(new_size, color_palette.data(), graphics::BufferUsage::StaticDraw);
        } else {
            m_palette_ssbo.update(0, new_size, color_palette.data());
        }
    }

    // Update category table SSBO
    if (!category_table.empty()) {
        size_t new_size = category_table.size() * sizeof(uint32_t);
        if (new_size > m_category_ssbo.size()) {
            m_category_ssbo.destroy();
            m_category_ssbo.create(new_size, category_table.data(), graphics::BufferUsage::StaticDraw);
        } else {
            m_category_ssbo.update(0, new_size, category_table.data());
        }
    }
}

std::vector<bool> MargolusSimulation::read_and_clear_dirty_chunks() {
    int total_chunks = m_num_chunks_x * m_num_chunks_y;
    std::vector<uint32_t> dirty_flags(total_chunks);
    std::vector<bool> result(total_chunks, false);

    // Read dirty flags from GPU
    if (m_dirty_chunks_ssbo.readback(0, total_chunks * sizeof(uint32_t), dirty_flags.data())) {
        for (int i = 0; i < total_chunks; i++) {
            result[i] = (dirty_flags[i] != 0);
        }

        // Clear dirty flags for next frame
        std::vector<uint32_t> zeros(total_chunks, 0);
        m_dirty_chunks_ssbo.update(0, total_chunks * sizeof(uint32_t), zeros.data());
    }

    return result;
}

} // namespace engine::simulation
