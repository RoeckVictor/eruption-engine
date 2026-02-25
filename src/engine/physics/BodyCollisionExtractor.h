#pragma once

#include "engine/graphics/Shader.h"
#include "engine/graphics/ShaderStorageBuffer.h"
#include <cstdint>
#include <random>
#include <vector>

namespace engine::graphics { class RenderContext; }
namespace engine::simulation { class PixelGrid; }
namespace engine::particles { class ParticleBuffer; }

namespace engine::physics {

class PixelBodyStamper;
class ColliderStamper;

struct ExtractedPixel {
    int x, y;
    uint32_t material;
};

// Extracts movable pixels that collided with rigidbodies during CA simulation.
// Works in conjunction with sim_step.comp which marks colliding pixels with
// FLAG_CONVERT_TO_PARTICLE. This class runs a compute shader to extract those
// pixels and spawn them as particles.
// Pipeline:
//   1. [sim_step runs, marks colliding movables]
//   2. extract() — GPU extracts marked pixels, clears them from grid
//   3. spawn_particles() — CPU reads extraction buffer, spawns particles
class BodyCollisionExtractor {
public:
    bool init(int grid_width, int grid_height, int max_extractions = 4096,
              float scatter_min = -30.0f, float scatter_max = 30.0f,
              float particle_lifetime = 5.0f);
    void shutdown();

    void extract(simulation::PixelGrid& grid, graphics::RenderContext& ctx);

    template<typename StamperT>
    void spawn_particles(const StamperT& stamper, particles::ParticleBuffer& particle_buffer);

    int last_extraction_count() const { return m_last_count; }

private:
    graphics::Shader m_extract_shader;
    graphics::ShaderStorageBuffer m_extraction_ssbo;

    int m_grid_width = 0;
    int m_grid_height = 0;
    int m_max_extractions = 0;
    int m_last_count = 0;
    float m_particle_lifetime = 5.0f;

    // CPU-side buffer for readback
    std::vector<uint32_t> m_readback_buffer;

    // Random number generation for particle spawning scatter
    std::mt19937 m_rng;
    std::uniform_real_distribution<float> m_scatter_dist;

    static constexpr int EXTRACTION_BINDING = 5;
};

}
