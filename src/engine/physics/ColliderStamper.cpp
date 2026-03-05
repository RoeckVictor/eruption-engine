#include "engine/physics/ColliderStamper.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/simulation/PixelGrid.h"
#include "engine/simulation/MaterialDefs.h"
#include "engine/particles/ParticleBuffer.h"
#include "engine/core/Log.h"
#include "engine/core/MathConstants.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace engine::physics {

ColliderStamper::ColliderStamper() {
    std::random_device rd;
    m_rng.seed(rd());
    m_scatter_dist = std::uniform_real_distribution<float>(-50.0f, 50.0f);
}

void ColliderStamper::configure(float scatter_min, float scatter_max, float particle_lifetime) {
    m_scatter_dist = std::uniform_real_distribution<float>(scatter_min, scatter_max);
    m_particle_lifetime = particle_lifetime;
}

void ColliderStamper::stamp_colliders(PhysicsWorld& world,
                                        simulation::PixelGrid& grid,
                                        float grid_origin_x, float grid_origin_y,
                                        particles::ParticleBuffer* particle_buffer) {
    m_stamps.clear();
    int body_count = 0;

    // Iterate through all bodies in the world
    world.for_each_body([&, this](b2BodyId body_id) {
        body_count++;
        if (!b2Body_IsValid(body_id)) return;

        float body_angle = b2Rot_GetAngle(b2Body_GetRotation(body_id));

        // Iterate through shapes attached to this body
        int shape_count = b2Body_GetShapeCount(body_id);
        b2ShapeId shapes[64];  // Max shapes per body
        int actual_count = b2Body_GetShapes(body_id, shapes, std::min(shape_count, 64));

        for (int i = 0; i < actual_count; i++) {
            b2ShapeId shape_id = shapes[i];
            if (!b2Shape_IsValid(shape_id)) continue;

            b2ShapeType type = b2Shape_GetType(shape_id);

            switch (type) {
                case b2_circleShape: {
                    b2Circle circle = b2Shape_GetCircle(shape_id);
                    stamp_circle(body_id, circle, body_angle, world, grid,
                                 grid_origin_x, grid_origin_y, particle_buffer);
                    break;
                }
                case b2_polygonShape: {
                    b2Polygon polygon = b2Shape_GetPolygon(shape_id);
                    stamp_polygon(body_id, polygon, body_angle, world, grid,
                                  grid_origin_x, grid_origin_y, particle_buffer);
                    break;
                }
                case b2_capsuleShape: {
                    b2Capsule capsule = b2Shape_GetCapsule(shape_id);
                    stamp_capsule(body_id, capsule, body_angle, world, grid,
                                  grid_origin_x, grid_origin_y, particle_buffer);
                    break;
                }
                default:
                    // Segments not yet supported
                    break;
            }
        }
    });

    // Debug logging disabled - enable if needed
    // if (body_count > 0 || !m_stamps.empty()) {
    //     ENGINE_LOG("ColliderStamper: Processed %d bodies, created %zu stamps",
    //                body_count, m_stamps.size());
    // }
}

void ColliderStamper::clear_colliders(simulation::PixelGrid& grid) {
    size_t pixel_size = grid.pixel_size();

    // Restore in reverse order
    for (int i = static_cast<int>(m_stamps.size()) - 1; i >= 0; i--) {
        auto& stamp = m_stamps[i];

        // Read current grid state for this region
        std::vector<uint8_t> current(stamp.original_pixels.size());
        grid.readback_region(stamp.region_x, stamp.region_y,
                             stamp.region_w, stamp.region_h,
                             current.data(), static_cast<int>(current.size()));

        // Only restore pixels that were actually stamped (inside the shape)
        // Pixels outside the shape may have been modified (e.g., extracted as particles)
        // and should NOT be restored to their pre-stamp state
        for (int y = 0; y < stamp.region_h; y++) {
            for (int x = 0; x < stamp.region_w; x++) {
                size_t pixel_idx = static_cast<size_t>(y) * stamp.region_w + x;
                if (stamp.was_stamped[pixel_idx]) {
                    // This pixel was inside the shape - restore original
                    size_t byte_offset = pixel_idx * pixel_size;
                    for (size_t b = 0; b < pixel_size; b++) {
                        current[byte_offset + b] = stamp.original_pixels[byte_offset + b];
                    }
                }
                // Pixels not stamped: keep current grid state (may have been extracted)
            }
        }

        grid.upload_both(stamp.region_x, stamp.region_y,
                         stamp.region_w, stamp.region_h,
                         current.data());
    }
    m_stamps.clear();
}

bool ColliderStamper::get_body_velocity_at_position(int x, int y, float& out_vx, float& out_vy) const {
    for (const auto& stamp : m_stamps) {
        if (x >= stamp.region_x && x < stamp.region_x + stamp.region_w &&
            y >= stamp.region_y && y < stamp.region_y + stamp.region_h) {

            // Compute outward direction from shape center to pixel
            float px = static_cast<float>(x) + 0.5f;
            float py = static_cast<float>(y) + 0.5f;
            float dx = px - stamp.center_x;
            float dy = py - stamp.center_y;
            float dist = std::sqrt(dx * dx + dy * dy);

            // Normalize direction (with fallback for center pixels)
            float nx = 0.0f, ny = -1.0f;  // Default: push upward
            if (dist > 0.1f) {
                nx = dx / dist;
                ny = dy / dist;
            }

            // Body speed magnitude
            float body_speed = std::sqrt(stamp.vel_x * stamp.vel_x + stamp.vel_y * stamp.vel_y);

            // Base outward push speed (scales with impact)
            float outward_speed = 30.0f + body_speed * 0.5f;

            // Combine: outward push + partial momentum transfer
            out_vx = nx * outward_speed + stamp.vel_x * 0.3f;
            out_vy = ny * outward_speed + stamp.vel_y * 0.3f;
            return true;
        }
    }
    return false;
}

void ColliderStamper::stamp_circle(b2BodyId body_id, const b2Circle& circle, float body_angle,
                                     PhysicsWorld& world, simulation::PixelGrid& grid,
                                     float grid_origin_x, float grid_origin_y,
                                     particles::ParticleBuffer* particle_buffer) {
    // Get body world position in pixels
    b2Vec2 body_pos = b2Body_GetPosition(body_id);
    float body_px = world.meters_to_pixels(body_pos.x);
    float body_py = world.meters_to_pixels(body_pos.y);

    // Transform circle center to world coordinates (accounting for body rotation)
    float cos_a = std::cos(body_angle);
    float sin_a = std::sin(body_angle);
    float center_local_px = world.meters_to_pixels(circle.center.x);
    float center_local_py = world.meters_to_pixels(circle.center.y);
    float center_world_x = body_px + center_local_px * cos_a - center_local_py * sin_a;
    float center_world_y = body_py + center_local_px * sin_a + center_local_py * cos_a;

    // Convert to grid coordinates
    float center_gx = center_world_x - grid_origin_x;
    // Flip Y: physics uses Y-up, pixel grid uses Y-down
    float center_gy = (grid.height() - 1) - (center_world_y - grid_origin_y);
    float radius_px = world.meters_to_pixels(circle.radius);

    // Compute grid-space AABB
    int min_gx = std::max(0, static_cast<int>(std::floor(center_gx - radius_px)));
    int min_gy = std::max(0, static_cast<int>(std::floor(center_gy - radius_px)));
    int max_gx = std::min(grid.width() - 1, static_cast<int>(std::ceil(center_gx + radius_px)));
    int max_gy = std::min(grid.height() - 1, static_cast<int>(std::ceil(center_gy + radius_px)));

    if (max_gx < min_gx || max_gy < min_gy) return;

    float radius_sq = radius_px * radius_px;
    auto inside_test = [center_gx, center_gy, radius_sq](float px, float py) -> bool {
        float dx = px - center_gx;
        float dy = py - center_gy;
        return dx * dx + dy * dy <= radius_sq;
    };

    stamp_region(body_id, min_gx, min_gy, max_gx, max_gy, center_gx, center_gy,
                 grid_origin_x, grid_origin_y, grid.height(),
                 inside_test, world, grid, particle_buffer);
}

void ColliderStamper::stamp_polygon(b2BodyId body_id, const b2Polygon& polygon, float body_angle,
                                      PhysicsWorld& world, simulation::PixelGrid& grid,
                                      float grid_origin_x, float grid_origin_y,
                                      particles::ParticleBuffer* particle_buffer) {
    // Get body world position in pixels
    b2Vec2 body_pos = b2Body_GetPosition(body_id);
    float body_px = world.meters_to_pixels(body_pos.x);
    float body_py = world.meters_to_pixels(body_pos.y);

    float cos_a = std::cos(body_angle);
    float sin_a = std::sin(body_angle);

    // Transform polygon vertices to world pixels, then to grid coordinates
    // Also compute centroid for particle physics
    std::vector<std::pair<float, float>> verts;
    verts.reserve(polygon.count);

    float min_gx_f = 1e9f, min_gy_f = 1e9f;
    float max_gx_f = -1e9f, max_gy_f = -1e9f;
    float centroid_gx = 0.0f, centroid_gy = 0.0f;

    for (int i = 0; i < polygon.count; i++) {
        float lx = world.meters_to_pixels(polygon.vertices[i].x);
        float ly = world.meters_to_pixels(polygon.vertices[i].y);

        float wx = body_px + lx * cos_a - ly * sin_a;
        float wy = body_py + lx * sin_a + ly * cos_a;

        float gx = wx - grid_origin_x;
        // Flip Y: physics uses Y-up, pixel grid uses Y-down
        float gy = (grid.height() - 1) - (wy - grid_origin_y);

        verts.emplace_back(gx, gy);
        centroid_gx += gx;
        centroid_gy += gy;

        min_gx_f = std::min(min_gx_f, gx);
        min_gy_f = std::min(min_gy_f, gy);
        max_gx_f = std::max(max_gx_f, gx);
        max_gy_f = std::max(max_gy_f, gy);
    }

    centroid_gx /= static_cast<float>(polygon.count);
    centroid_gy /= static_cast<float>(polygon.count);

    int min_gx = std::max(0, static_cast<int>(std::floor(min_gx_f)));
    int min_gy = std::max(0, static_cast<int>(std::floor(min_gy_f)));
    int max_gx = std::min(grid.width() - 1, static_cast<int>(std::ceil(max_gx_f)));
    int max_gy = std::min(grid.height() - 1, static_cast<int>(std::ceil(max_gy_f)));

    if (max_gx < min_gx || max_gy < min_gy) return;

    // Point-in-polygon test using ray casting
    int n = static_cast<int>(verts.size());
    auto inside_test = [&verts, n](float px, float py) -> bool {
        bool inside = false;
        for (int i = 0, j = n - 1; i < n; j = i++) {
            float xi = verts[i].first, yi = verts[i].second;
            float xj = verts[j].first, yj = verts[j].second;

            if ((yi > py) != (yj > py) &&
                px < (xj - xi) * (py - yi) / (yj - yi) + xi) {
                inside = !inside;
            }
        }
        return inside;
    };

    stamp_region(body_id, min_gx, min_gy, max_gx, max_gy, centroid_gx, centroid_gy,
                 grid_origin_x, grid_origin_y, grid.height(),
                 inside_test, world, grid, particle_buffer);
}

void ColliderStamper::stamp_capsule(b2BodyId body_id, const b2Capsule& capsule, float body_angle,
                                     PhysicsWorld& world, simulation::PixelGrid& grid,
                                     float grid_origin_x, float grid_origin_y,
                                     particles::ParticleBuffer* particle_buffer) {
    // Get body world position in pixels
    b2Vec2 body_pos = b2Body_GetPosition(body_id);
    float body_px = world.meters_to_pixels(body_pos.x);
    float body_py = world.meters_to_pixels(body_pos.y);

    float cos_a = std::cos(body_angle);
    float sin_a = std::sin(body_angle);

    // Transform capsule endpoints to world pixels, then to grid coordinates
    float c1_local_px = world.meters_to_pixels(capsule.center1.x);
    float c1_local_py = world.meters_to_pixels(capsule.center1.y);
    float c2_local_px = world.meters_to_pixels(capsule.center2.x);
    float c2_local_py = world.meters_to_pixels(capsule.center2.y);

    float c1_world_x = body_px + c1_local_px * cos_a - c1_local_py * sin_a;
    float c1_world_y = body_py + c1_local_px * sin_a + c1_local_py * cos_a;
    float c2_world_x = body_px + c2_local_px * cos_a - c2_local_py * sin_a;
    float c2_world_y = body_py + c2_local_px * sin_a + c2_local_py * cos_a;

    // Convert to grid coordinates (flip Y)
    float c1_gx = c1_world_x - grid_origin_x;
    float c1_gy = (grid.height() - 1) - (c1_world_y - grid_origin_y);
    float c2_gx = c2_world_x - grid_origin_x;
    float c2_gy = (grid.height() - 1) - (c2_world_y - grid_origin_y);

    float radius_px = world.meters_to_pixels(capsule.radius);

    // Compute centroid (midpoint of the capsule)
    float centroid_gx = (c1_gx + c2_gx) * 0.5f;
    float centroid_gy = (c1_gy + c2_gy) * 0.5f;

    // Compute AABB
    float min_gx_f = std::min(c1_gx, c2_gx) - radius_px;
    float min_gy_f = std::min(c1_gy, c2_gy) - radius_px;
    float max_gx_f = std::max(c1_gx, c2_gx) + radius_px;
    float max_gy_f = std::max(c1_gy, c2_gy) + radius_px;

    int min_gx = std::max(0, static_cast<int>(std::floor(min_gx_f)));
    int min_gy = std::max(0, static_cast<int>(std::floor(min_gy_f)));
    int max_gx = std::min(grid.width() - 1, static_cast<int>(std::ceil(max_gx_f)));
    int max_gy = std::min(grid.height() - 1, static_cast<int>(std::ceil(max_gy_f)));

    if (max_gx < min_gx || max_gy < min_gy) return;

    // Point-in-capsule test: distance to line segment <= radius
    float radius_sq = radius_px * radius_px;
    auto inside_test = [c1_gx, c1_gy, c2_gx, c2_gy, radius_sq](float px, float py) -> bool {
        // Vector from c1 to c2
        float dx = c2_gx - c1_gx;
        float dy = c2_gy - c1_gy;
        float len_sq = dx * dx + dy * dy;

        // Project point onto line segment
        float t = 0.0f;
        if (len_sq > engine::EPSILON) {
            t = ((px - c1_gx) * dx + (py - c1_gy) * dy) / len_sq;
            t = std::max(0.0f, std::min(1.0f, t));
        }

        // Closest point on segment
        float closest_x = c1_gx + t * dx;
        float closest_y = c1_gy + t * dy;

        // Distance squared to closest point
        float dist_x = px - closest_x;
        float dist_y = py - closest_y;
        float dist_sq = dist_x * dist_x + dist_y * dist_y;

        return dist_sq <= radius_sq;
    };

    stamp_region(body_id, min_gx, min_gy, max_gx, max_gy, centroid_gx, centroid_gy,
                 grid_origin_x, grid_origin_y, grid.height(),
                 inside_test, world, grid, particle_buffer);
}

void ColliderStamper::stamp_region(b2BodyId body_id, int min_gx, int min_gy, int max_gx, int max_gy,
                                     float center_gx, float center_gy,
                                     float grid_origin_x, float grid_origin_y, int grid_height,
                                     const std::function<bool(float, float)>& inside_test,
                                     PhysicsWorld& world, simulation::PixelGrid& grid,
                                     particles::ParticleBuffer* particle_buffer) {
    int region_w = max_gx - min_gx + 1;
    int region_h = max_gy - min_gy + 1;
    int displaced_count = 0;
    int pixels_inside_shape = 0;
    int empty_count = 0, static_count = 0, movable_count = 0;

    // Get body velocity (flip Y: physics Y-up -> grid Y-down)
    b2Vec2 body_vel = b2Body_GetLinearVelocity(body_id);
    float vel_px = world.meters_to_pixels(body_vel.x);
    float vel_py = -world.meters_to_pixels(body_vel.y);  // Negate for Y-down grid

    // Read back original pixels
    ColliderStamp stamp;
    stamp.region_x = min_gx;
    stamp.region_y = min_gy;
    stamp.region_w = region_w;
    stamp.region_h = region_h;
    stamp.vel_x = vel_px;
    stamp.vel_y = vel_py;
    stamp.center_x = center_gx;
    stamp.center_y = center_gy;
    size_t pixel_size = grid.pixel_size();
    stamp.original_pixels.resize(region_w * region_h * pixel_size);
    stamp.was_stamped.resize(region_w * region_h, false);  // Track which pixels were actually stamped
    grid.readback_region(min_gx, min_gy, region_w, region_h,
                         stamp.original_pixels.data(),
                         static_cast<int>(stamp.original_pixels.size()));

    // Create stamped copy
    std::vector<uint8_t> stamped(stamp.original_pixels);

    // Rasterize the shape
    for (int gy = min_gy; gy <= max_gy; gy++) {
        for (int gx = min_gx; gx <= max_gx; gx++) {
            // Test pixel center
            float px = static_cast<float>(gx) + 0.5f;
            float py = static_cast<float>(gy) + 0.5f;

            if (!inside_test(px, py)) continue;

            pixels_inside_shape++;

            int local_x = gx - min_gx;
            int local_y = gy - min_gy;
            size_t ri = (static_cast<size_t>(local_y) * region_w + local_x) * pixel_size;

            uint8_t original_mat = stamp.original_pixels[ri + 0];
            uint8_t original_cat = stamp.original_pixels[ri + 1];

            // Count pixel types for debugging
            if (original_cat == simulation::CAT_EMPTY || original_mat == 0) {
                empty_count++;
            } else if (original_cat == simulation::CAT_STATIC) {
                static_count++;
            } else {
                movable_count++;
            }

            // Check if displacing a movable material
            if (particle_buffer &&
                original_mat != 0 &&
                (original_cat == simulation::CAT_POWDER ||
                 original_cat == simulation::CAT_LIQUID ||
                 original_cat == simulation::CAT_GAS)) {

                // Compute outward direction from shape center to pixel
                float dx = px - center_gx;
                float dy = py - center_gy;
                float dist = std::sqrt(dx * dx + dy * dy);

                // Normalize direction (with fallback for center pixels)
                float nx = 0.0f, ny = -1.0f;  // Default: push upward
                if (dist > 0.1f) {
                    nx = dx / dist;
                    ny = dy / dist;
                }

                // Body speed magnitude
                float body_speed = std::sqrt(vel_px * vel_px + vel_py * vel_py);

                // Base outward push speed (scales with impact)
                float outward_speed = 30.0f + body_speed * 0.5f;

                // Random angular scatter (±45 degrees)
                float angle_scatter = m_scatter_dist(m_rng) * 0.015f;  // ~±0.75 rad
                float cos_s = std::cos(angle_scatter);
                float sin_s = std::sin(angle_scatter);
                float nx_rot = nx * cos_s - ny * sin_s;
                float ny_rot = nx * sin_s + ny * cos_s;

                // Combine: outward push + partial momentum transfer + random scatter
                // Velocity in grid-space (Y-down) for particle simulation
                float final_vx = nx_rot * outward_speed + vel_px * 0.3f + m_scatter_dist(m_rng) * 0.3f;
                float final_vy = ny_rot * outward_speed + vel_py * 0.3f + m_scatter_dist(m_rng) * 0.3f;

                // Position in grid coords (Y-down) - renderer will convert to world
                particles::SpawnRequest req{};
                req.px = static_cast<float>(gx) + 0.5f;
                req.py = static_cast<float>(gy) + 0.5f;
                req.vx = final_vx;
                req.vy = final_vy;
                req.material = original_mat;
                req.lifetime = m_particle_lifetime;

                // Extract color from original pixel (bytes 4-7 for 8-byte pixels)
                if (pixel_size >= 8) {
                    uint8_t r = stamp.original_pixels[ri + 4];
                    uint8_t g = stamp.original_pixels[ri + 5];
                    uint8_t b = stamp.original_pixels[ri + 6];
                    uint8_t a = stamp.original_pixels[ri + 7];
                    req.color = (static_cast<uint32_t>(r)) |
                                (static_cast<uint32_t>(g) << 8) |
                                (static_cast<uint32_t>(b) << 16) |
                                (static_cast<uint32_t>(a) << 24);
                } else {
                    req.color = 0xFFFFFFFF;
                }

                particle_buffer->spawn(req);
                displaced_count++;

                // Clear from original so it doesn't get restored (sim data + color)
                stamp.original_pixels[ri + 0] = 0;
                stamp.original_pixels[ri + 1] = 0;
                stamp.original_pixels[ri + 2] = 0;
                stamp.original_pixels[ri + 3] = 0;
                if (pixel_size >= 8) {
                    stamp.original_pixels[ri + 4] = 0;
                    stamp.original_pixels[ri + 5] = 0;
                    stamp.original_pixels[ri + 6] = 0;
                    stamp.original_pixels[ri + 7] = 0;
                }
            }

            // Mark this pixel as actually stamped (inside the shape)
            size_t pixel_idx = static_cast<size_t>(local_y) * region_w + local_x;
            stamp.was_stamped[pixel_idx] = true;

            // Stamp as static with FLAG_RIGIDBODY
            // Use material ID 255 as a "collider" placeholder (won't render)
            stamped[ri + 0] = 255;  // Material ID (unused for rendering)
            stamped[ri + 1] = simulation::CAT_STATIC;
            stamped[ri + 2] = 128;  // Temperature
            stamped[ri + 3] = simulation::PixelFlags::FLAG_RIGIDBODY;
        }
    }

    // Upload stamped region
    grid.upload_both(min_gx, min_gy, region_w, region_h, stamped.data());

    m_stamps.push_back(std::move(stamp));
}

} // namespace engine::physics
