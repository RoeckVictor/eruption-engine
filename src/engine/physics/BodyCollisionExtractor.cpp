#include "engine/physics/BodyCollisionExtractor.h"
#include "engine/physics/PixelBodyStamper.h"
#include "engine/physics/ColliderStamper.h"
#include "engine/simulation/PixelGrid.h"
#include "engine/particles/ParticleBuffer.h"
#include "engine/graphics/RenderContext.h"
#include "engine/rhi/RHITypes.h"
#include "engine/core/Log.h"
#include <random>

namespace engine::physics {

bool BodyCollisionExtractor::init(int grid_width, int grid_height, int max_extractions,
                                   float scatter_min, float scatter_max,
                                   float particle_lifetime) {
    if (grid_width <= 0 || grid_height <= 0 || max_extractions <= 0) {
        ENGINE_ERR("BodyCollisionExtractor::init() - Invalid parameters (%dx%d, max=%d)",
                   grid_width, grid_height, max_extractions);
        return false;
    }

    m_grid_width = grid_width;
    m_grid_height = grid_height;
    m_max_extractions = max_extractions;
    m_particle_lifetime = particle_lifetime;

    // Load extraction compute shader
    if (!m_extract_shader.load_compute("shaders/body_collision_extract.comp")) {
        ENGINE_ERR("BodyCollisionExtractor: Failed to load body_collision_extract.comp");
        return false;
    }

    // Set constant uniforms
    m_extract_shader.use();
    m_extract_shader.set_int("u_grid_width", grid_width);
    m_extract_shader.set_int("u_grid_height", grid_height);

    // Create extraction buffer SSBO
    // Layout: [count (1 uint), capacity (1 uint), entries (3 uints each: pos, material, color)]
    size_t buffer_size = (2 + max_extractions * 3) * sizeof(uint32_t);

    // Initialize with count=0 and capacity
    std::vector<uint32_t> init_data(2 + max_extractions * 3, 0);
    init_data[0] = 0;  // count
    init_data[1] = static_cast<uint32_t>(max_extractions);  // capacity

    if (!m_extraction_ssbo.create(buffer_size, init_data.data(),
                                   graphics::BufferUsage::StreamRead)) {
        ENGINE_ERR("BodyCollisionExtractor: Failed to create extraction SSBO");
        m_extract_shader.destroy();
        return false;
    }

    // Allocate readback buffer
    m_readback_buffer.resize(2 + max_extractions * 3);

    // Initialize RNG for particle scatter
    std::random_device rd;
    m_rng.seed(rd());
    m_scatter_dist = std::uniform_real_distribution<float>(scatter_min, scatter_max);

    return true;
}

void BodyCollisionExtractor::shutdown() {
    m_extract_shader.destroy();
    m_extraction_ssbo.destroy();
    m_readback_buffer.clear();
}

void BodyCollisionExtractor::extract(simulation::PixelGrid& grid, graphics::RenderContext& ctx) {
    // Reset extraction count to 0
    uint32_t zero = 0;
    m_extraction_ssbo.update(0, sizeof(uint32_t), &zero);

    m_extract_shader.use();

    // Set pixel size uniform
    m_extract_shader.set_uint("u_pixel_size", static_cast<uint32_t>(grid.pixel_size()));

    // Bind pixel grid SSBOs - must use current read buffer after simulation
    // The shader reads from binding 0 (pixels_a) and writes to both
    int read_idx = grid.read_idx();
    grid.ssbo(read_idx).bind_base(0);      // Current read buffer -> binding 0
    grid.ssbo(1 - read_idx).bind_base(1);  // Write buffer -> binding 1

    // Bind extraction buffer
    m_extraction_ssbo.bind_base(EXTRACTION_BINDING);

    // Dispatch compute shader
    int groups_x = (m_grid_width + 15) / 16;
    int groups_y = (m_grid_height + 15) / 16;
    ctx.dispatch_compute(groups_x, groups_y, 1, rhi::BarrierFlags::StorageBuffer);

    m_last_count = 0;  // Will be set after readback in spawn_particles()
}

template<typename StamperT>
void BodyCollisionExtractor::spawn_particles(const StamperT& stamper,
                                              particles::ParticleBuffer& particle_buffer) {
    // Read back extraction buffer
    if (!m_extraction_ssbo.readback(0, m_readback_buffer.size() * sizeof(uint32_t),
                                     m_readback_buffer.data())) {
        ENGINE_ERR("BodyCollisionExtractor: Failed to read back extraction buffer");
        return;
    }

    uint32_t count = m_readback_buffer[0];
    uint32_t capacity = m_readback_buffer[1];

    if (count > capacity) {
        ENGINE_LOG_WARN("BodyCollisionExtractor: Extraction overflow (%u > %u), some pixels lost",
                        count, capacity);
        count = capacity;
    }

    m_last_count = static_cast<int>(count);

    if (count > 0) {
        ENGINE_LOG("BodyCollisionExtractor: extracted %u pixels (capacity=%u)", count, capacity);
    }

    if (count == 0) return;

    // Process each extracted pixel (3 uints per entry: pos, material, color)
    for (uint32_t i = 0; i < count; i++) {
        uint32_t entry_offset = 2 + i * 3;
        uint32_t packed_pos = m_readback_buffer[entry_offset + 0];
        uint32_t material = m_readback_buffer[entry_offset + 1];
        uint32_t color = m_readback_buffer[entry_offset + 2];

        // Unpack position (grid coordinates, Y-down)
        int gx = static_cast<int>(packed_pos & 0xFFFF);
        int gy = static_cast<int>((packed_pos >> 16) & 0xFFFF);

        // Look up body velocity at this position (returns grid-space velocity)
        float vel_x = 0.0f;
        float vel_y = 0.0f;
        stamper.get_body_velocity_at_position(gx, gy, vel_x, vel_y);

        // Add some scatter for visual variety
        float scatter_x = m_scatter_dist(m_rng);
        float scatter_y = m_scatter_dist(m_rng);

        // Spawn particle in grid coords (Y-down) - renderer will convert to world
        particles::SpawnRequest req{};
        req.px = static_cast<float>(gx) + 0.5f;
        req.py = static_cast<float>(gy) + 0.5f;
        req.vx = vel_x + scatter_x;
        req.vy = vel_y + scatter_y;
        req.material = material;
        req.lifetime = m_particle_lifetime;
        req.color = color;

        particle_buffer.spawn(req);
    }
}

// Explicit template instantiations
template void BodyCollisionExtractor::spawn_particles<PixelBodyStamper>(
    const PixelBodyStamper&, particles::ParticleBuffer&);
template void BodyCollisionExtractor::spawn_particles<ColliderStamper>(
    const ColliderStamper&, particles::ParticleBuffer&);

} // namespace engine::physics
