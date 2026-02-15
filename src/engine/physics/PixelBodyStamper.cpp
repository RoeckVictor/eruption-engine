#include "engine/physics/PixelBodyStamper.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/physics/PixelBody.h"
#include "engine/simulation/PixelGrid.h"
#include "engine/simulation/MaterialDefs.h"
#include "engine/particles/ParticleBuffer.h"
#include "engine/core/Log.h"
#include <algorithm>
#include <cmath>

namespace engine::physics {

PixelBodyStamper::PixelBodyStamper() {
    // Initialize random number generator with proper seeding
    std::random_device rd;
    m_rng.seed(rd());
    m_scatter_dist = std::uniform_real_distribution<float>(-50.0f, 50.0f);
}

void PixelBodyStamper::stamp_all(const std::vector<PixelBody*>& bodies,
                                   PhysicsWorld& world,
                                   simulation::PixelGrid& grid,
                                   particles::ParticleBuffer* particle_buffer) {
    m_body_stamps.clear();

    for (PixelBody* body_ptr : bodies) {
        PixelBody& body = *body_ptr;
        if (!b2Body_IsValid(body.body_id())) continue;
        if (body.pixel_count() == 0) continue;

        float bx = body.world_x(world);
        float by = body.world_y(world);
        float angle = body.rotation(world);
        float cos_a = std::cos(angle);
        float sin_a = std::sin(angle);
        float cx = body.local_center_x();
        float cy = body.local_center_y();

        // Get body velocity for particle spawning
        b2Vec2 body_vel = world.get_body_linear_velocity(body.body_id());

        int bw = body.width();
        int bh = body.height();
        const uint8_t* materials = body.materials();
        const uint8_t* categories = body.categories();

        // Compute world-space AABB from the 4 corners of the body's bounding box.
        // This is both faster and more accurate than iterating all pixels.
        float corners[4][2] = {
            {-cx,      -cy},
            {bw - cx,  -cy},
            {bw - cx,  bh - cy},
            {-cx,      bh - cy}
        };

        float fmin_wx = 1e9f, fmin_wy = 1e9f;
        float fmax_wx = -1e9f, fmax_wy = -1e9f;
        for (auto& c : corners) {
            float wx = bx + c[0] * cos_a - c[1] * sin_a;
            float wy = by + c[0] * sin_a + c[1] * cos_a;
            fmin_wx = std::min(fmin_wx, wx);
            fmin_wy = std::min(fmin_wy, wy);
            fmax_wx = std::max(fmax_wx, wx);
            fmax_wy = std::max(fmax_wy, wy);
        }

        int min_wx = std::max(static_cast<int>(std::floor(fmin_wx)), 0);
        int min_wy = std::max(static_cast<int>(std::floor(fmin_wy)), 0);
        int max_wx = std::min(static_cast<int>(std::floor(fmax_wx)), grid.width() - 1);
        int max_wy = std::min(static_cast<int>(std::floor(fmax_wy)), grid.height() - 1);

        if (max_wx < min_wx || max_wy < min_wy) continue;

        int region_w = max_wx - min_wx + 1;
        int region_h = max_wy - min_wy + 1;

        // Guard against integer overflow in region byte count
        size_t region_pixels = static_cast<size_t>(region_w) * static_cast<size_t>(region_h);
        if (region_pixels > static_cast<size_t>(INT_MAX / 4)) {
            ENGINE_ERR("PixelBodyStamper: region too large (%dx%d), skipping body", region_w, region_h);
            continue;
        }
        int region_bytes = static_cast<int>(region_pixels * 4);

        // Single readback for the entire body AABB
        BodyStamp stamp_rec;
        stamp_rec.region_x = min_wx;
        stamp_rec.region_y = min_wy;
        stamp_rec.region_w = region_w;
        stamp_rec.region_h = region_h;
        stamp_rec.original_pixels.resize(region_bytes);
        grid.readback_region(min_wx, min_wy, region_w, region_h,
                             stamp_rec.original_pixels.data(), region_bytes);

        // Copy the region for modification (stamp body pixels into it)
        std::vector<uint8_t> stamped(stamp_rec.original_pixels);

        // Inverse mapping: iterate world AABB pixels and reverse-transform into
        // body local space. This guarantees every world pixel is covered (no gaps
        // from rotation rounding, unlike the forward-mapping approach).
        for (int wy = min_wy; wy <= max_wy; wy++) {
            for (int wx = min_wx; wx <= max_wx; wx++) {
                // World pixel center → body-local coordinates (inverse rotation)
                float dwx = (static_cast<float>(wx) + 0.5f) - bx;
                float dwy = (static_cast<float>(wy) + 0.5f) - by;
                float lx_f = dwx * cos_a + dwy * sin_a + cx;
                float ly_f = -dwx * sin_a + dwy * cos_a + cy;

                int lx = static_cast<int>(std::floor(lx_f));
                int ly = static_cast<int>(std::floor(ly_f));

                if (lx < 0 || lx >= bw || ly < 0 || ly >= bh) continue;

                int local_idx = ly * bw + lx;
                uint8_t mat = materials[local_idx];
                if (mat == 0) continue;

                // Validate coordinates are within region bounds (before calculation)
                int local_x = wx - min_wx;
                int local_y = wy - min_wy;

                if (local_x < 0 || local_x >= region_w ||
                    local_y < 0 || local_y >= region_h) {
                    ENGINE_ERR("Coordinate out of region bounds: local=(%d,%d), region=(%d,%d)",
                               local_x, local_y, region_w, region_h);
                    continue;
                }

                // Calculate buffer index using size_t arithmetic to prevent overflow
                size_t ri = (static_cast<size_t>(local_y) * static_cast<size_t>(region_w)
                           + static_cast<size_t>(local_x)) * 4;

                // Validate buffer bounds before accessing (defense in depth)
                if (ri + 3 >= stamped.size()) {
                    ENGINE_ERR("Buffer overflow prevented in stamp_all: ri=%zu, stamped.size()=%zu",
                               ri, stamped.size());
                    continue; // Skip this pixel
                }

                // Check if we're displacing a movable material (powder, liquid, gas)
                if (particle_buffer) {
                    // Validate original_pixels buffer access
                    if (ri + 3 >= stamp_rec.original_pixels.size()) {
                        ENGINE_ERR("Buffer overflow prevented in stamp_all (original_pixels): ri=%zu, size=%zu",
                                   ri, stamp_rec.original_pixels.size());
                        continue;
                    }

                    uint8_t original_mat = stamp_rec.original_pixels[ri + 0];
                    uint8_t original_cat = stamp_rec.original_pixels[ri + 1];

                    // Spawn particle if displacing a movable material (not air, not static)
                    if (original_mat != 0 &&
                        (original_cat == simulation::CAT_POWDER ||
                         original_cat == simulation::CAT_LIQUID ||
                         original_cat == simulation::CAT_GAS)) {

                        // Spawn particle with body velocity plus some scatter
                        // Add random scatter to make splashes look more natural
                        float scatter_x = m_scatter_dist(m_rng); // -50 to +50 pixels/sec
                        float scatter_y = m_scatter_dist(m_rng);

                        particles::SpawnRequest req{};
                        req.px = static_cast<float>(wx) + 0.5f;
                        req.py = static_cast<float>(wy) + 0.5f;
                        req.vx = body_vel.x + scatter_x;
                        req.vy = body_vel.y + scatter_y;
                        req.material = original_mat;
                        req.lifetime = 5.0f; // 5 seconds before auto-settling

                        particle_buffer->spawn(req);

                        // Clear the pixel from original_pixels so it doesn't get restored by clear_all()
                        // This prevents duplication (particle + grid pixel)
                        stamp_rec.original_pixels[ri + 0] = 0; // material = AIR
                        stamp_rec.original_pixels[ri + 1] = 0; // category = EMPTY
                        stamp_rec.original_pixels[ri + 2] = 0; // temperature = 0
                        stamp_rec.original_pixels[ri + 3] = 0; // flags = 0
                    }
                }

                stamped[ri + 0] = mat;                      // R = material ID
                stamped[ri + 1] = categories[local_idx];   // G = category
                stamped[ri + 2] = 128;                     // B = temperature (default)
                stamped[ri + 3] = 0;                       // A = reserved
            }
        }

        // Single upload of the stamped region
        grid.upload_both(min_wx, min_wy, region_w, region_h, stamped.data());

        m_body_stamps.push_back(std::move(stamp_rec));
    }
}

void PixelBodyStamper::clear_all(simulation::PixelGrid& grid) {
    // Restore in reverse stamp order to handle overlapping bodies correctly.
    // When bodies overlap, later stamps capture earlier bodies' pixels as
    // "originals." Clearing in reverse ensures the earliest stamp (which
    // captured the true clean grid) overwrites any leaked body pixels.
    for (int i = static_cast<int>(m_body_stamps.size()) - 1; i >= 0; i--) {
        auto& stamp = m_body_stamps[i];
        grid.upload_both(stamp.region_x, stamp.region_y,
                         stamp.region_w, stamp.region_h,
                         stamp.original_pixels.data());
    }
    m_body_stamps.clear();
}

} // namespace engine::physics
