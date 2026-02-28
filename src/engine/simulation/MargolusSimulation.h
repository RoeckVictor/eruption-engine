#pragma once

#include "engine/graphics/Shader.h"
#include "engine/graphics/ShaderStorageBuffer.h"
#include "engine/simulation/MaterialDefs.h"
#include <functional>
#include <vector>
#include <cstdint>

namespace engine::graphics { class RenderContext; }

namespace engine::simulation {

class PixelGrid;

/// Runs a Margolus-neighborhood cellular automata simulation via compute shader.
///
/// The simulation processes 2x2 blocks across 4 phases per step, ensuring all
/// adjacent cells interact. The caller provides the material table and compute
/// shader path; this class handles SSBO packing, ping-pong dispatch, and memory barriers.
///
/// The compute shader is game-owned: the engine provides the orchestrator,
/// and the game provides the actual simulation rules via the shader path.
///
/// Dirty chunk tracking: The simulation tracks which chunks had pixel movement,
/// allowing terrain collider generation to only update affected chunks.
class MargolusSimulation {
public:
    using UniformSetupCallback = std::function<void(graphics::Shader&)>;

    /// Initialize with pre-compiled GPU tables.
    /// @param material_table    Compiled material table (256 * 2 uint32s).
    /// @param interaction_table Compiled interaction table (N * 6 uint32s).
    /// @param color_palette     Color palette (256 uint32s, 0xRRGGBBAA per material).
    /// @param category_table    Compiled category table (16 * 10 uint32s).
    /// @param grid_width       Width of the pixel grid in pixels.
    /// @param grid_height      Height of the pixel grid in pixels.
    /// @param shader_path      Path to the simulation compute shader (.comp).
    /// @param setup_uniforms   Optional callback to set game-specific uniforms.
    /// @param chunk_size_x     Chunk width for dirty tracking.
    /// @param chunk_size_y     Chunk height for dirty tracking.
    bool init(const std::vector<uint32_t>& material_table,
              const std::vector<uint32_t>& interaction_table,
              const std::vector<uint32_t>& color_palette,
              const std::vector<uint32_t>& category_table,
              int grid_width, int grid_height, const char* shader_path,
              const UniformSetupCallback& setup_uniforms = nullptr,
              int chunk_size_x = 32, int chunk_size_y = 32);

    void shutdown();

    /// Run one full simulation step (4 Margolus phases).
    void simulate(PixelGrid& grid, graphics::RenderContext& ctx);

    /// Read dirty chunk flags from GPU and clear them for next frame.
    /// Returns a vector of bools, one per chunk (row-major order).
    /// Chunks are indexed as: chunk_index = cy * num_chunks_x + cx
    std::vector<bool> read_and_clear_dirty_chunks();

    /// Get chunk dimensions
    int chunk_size_x() const { return m_chunk_size_x; }
    int chunk_size_y() const { return m_chunk_size_y; }
    int num_chunks_x() const { return m_num_chunks_x; }
    int num_chunks_y() const { return m_num_chunks_y; }

    /// Update material, interaction, and category tables at runtime.
    /// Call this after recompiling the MaterialLibrary or CategoryLibrary.
    /// @param material_table    Compiled material table (256 * 2 uint32s).
    /// @param interaction_table Compiled interaction table (N * 6 uint32s).
    /// @param color_palette     Color palette (256 uint32s, 0xRRGGBBAA per material).
    /// @param category_table    Compiled category table (16 * 10 uint32s).
    void update_tables(const std::vector<uint32_t>& material_table,
                       const std::vector<uint32_t>& interaction_table,
                       const std::vector<uint32_t>& color_palette,
                       const std::vector<uint32_t>& category_table);

private:
    graphics::ShaderStorageBuffer m_material_ssbo;
    graphics::ShaderStorageBuffer m_dirty_chunks_ssbo;
    graphics::ShaderStorageBuffer m_interaction_ssbo;  // Interaction table (binding 4)
    graphics::ShaderStorageBuffer m_palette_ssbo;      // Color palette (binding 5)
    graphics::ShaderStorageBuffer m_category_ssbo;     // Category table (binding 6)
    graphics::Shader m_sim_shader;

    int m_grid_width = 0;
    int m_grid_height = 0;
    int m_chunk_size_x = 32;
    int m_chunk_size_y = 32;
    int m_num_chunks_x = 0;
    int m_num_chunks_y = 0;
    bool m_has_interactions = false;  // True if using v2 format with interactions
};

} // namespace engine::simulation
