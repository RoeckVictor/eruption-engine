#include "engine/physics/TerrainColliderManager.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/physics/PixelBody.h"
#include "engine/physics/ContourGenerator.h"
#include "engine/simulation/PixelGrid.h"
#include "engine/simulation/MaterialDefs.h"
#include "engine/core/Log.h"
#include "engine/core/MathConstants.h"
#include <algorithm>
#include <cmath>
#include <memory>

namespace engine::physics {

bool TerrainColliderManager::init(PhysicsWorld& world, int chunk_size_x, int chunk_size_y) {
    if (!world.valid()) {
        ENGINE_ERR("TerrainColliderManager::init() - Invalid PhysicsWorld provided");
        return false;
    }

    m_world = &world;
    m_chunk_size_x = chunk_size_x;
    m_chunk_size_y = chunk_size_y;
    return true;
}

void TerrainColliderManager::shutdown() {
    if (!m_world) {
        ENGINE_LOG("TerrainColliderManager::shutdown() called before init() or after previous shutdown()");
        return;
    }

    // Destroy all terrain colliders
    for (auto& [coord, chunk] : m_terrain_chunks) {
        if (b2Body_IsValid(chunk.body_id)) {
            m_world->destroy_body(chunk.body_id);
        }
    }
    m_terrain_chunks.clear();
    m_world = nullptr;
}

void TerrainColliderManager::mark_dirty_region(int x, int y, int w, int h) {
    // Convert pixel rectangle to chunk coordinate range
    int min_cx = static_cast<int>(std::floor(static_cast<float>(x) / m_chunk_size_x));
    int min_cy = static_cast<int>(std::floor(static_cast<float>(y) / m_chunk_size_y));
    int max_cx = static_cast<int>(std::floor(static_cast<float>(x + w - 1) / m_chunk_size_x));
    int max_cy = static_cast<int>(std::floor(static_cast<float>(y + h - 1) / m_chunk_size_y));

    // Mark all chunks in the region (O(1) lookup per chunk)
    for (int cy = min_cy; cy <= max_cy; cy++) {
        for (int cx = min_cx; cx <= max_cx; cx++) {
            ChunkCoord coord{cx, cy};
            auto it = m_terrain_chunks.find(coord);
            if (it != m_terrain_chunks.end()) {
                it->second.dirty = true;
            }
        }
    }
}

void TerrainColliderManager::mark_dirty_near_bodies(const std::vector<PixelBody*>& bodies,
                                                      PhysicsWorld& world,
                                                      float margin) {
    for (PixelBody* body_ptr : bodies) {
        if (!b2Body_IsValid(body_ptr->body_id())) continue;
        if (body_ptr->pixel_count() == 0) continue;

        float bx = body_ptr->world_x(world);
        float by = body_ptr->world_y(world);
        int bw = body_ptr->width();
        int bh = body_ptr->height();
        float half_diag = std::sqrt(static_cast<float>(bw * bw + bh * bh)) * 0.5f;
        float extent = half_diag + margin;

        mark_dirty_region(
            static_cast<int>(bx - extent), static_cast<int>(by - extent),
            static_cast<int>(extent * 2.0f), static_cast<int>(extent * 2.0f));
    }
}

void TerrainColliderManager::update_terrain_colliders(simulation::PixelGrid& grid,
                                                       const EntityTransform& transform) {
    int grid_h = grid.height();

    // Calculate the number of chunks covering the entire grid
    int num_chunks_x = (grid.width() + m_chunk_size_x - 1) / m_chunk_size_x;
    int num_chunks_y = (grid.height() + m_chunk_size_y - 1) / m_chunk_size_y;

    // Precompute entity rotation for body-local → world conversion (debug verts only)
    float e_rot_rad = transform.world_rotation_deg * DEG_TO_RAD;
    float e_cos = std::cos(e_rot_rad);
    float e_sin = std::sin(e_rot_rad);

    // Process all chunks in the grid
    for (int cy = 0; cy < num_chunks_y; cy++) {
        for (int cx = 0; cx < num_chunks_x; cx++) {
            int chunk_px = cx * m_chunk_size_x;
            int chunk_py = cy * m_chunk_size_y;

            // Find or create terrain chunk entry (O(1) hash map lookup)
            ChunkCoord coord{cx, cy};
            auto it = m_terrain_chunks.find(coord);

            TerrainChunk* chunk = nullptr;
            if (it != m_terrain_chunks.end()) {
                chunk = &it->second;
            } else {
                // Create new chunk
                auto [inserted_it, success] = m_terrain_chunks.emplace(coord, TerrainChunk{});
                chunk = &inserted_it->second;
            }

            chunk->active = true;

            // Only regenerate dirty chunks
            if (!chunk->dirty) continue;
            chunk->dirty = false;

            // Destroy old collider before regenerating
            if (b2Body_IsValid(chunk->body_id)) {
                m_world->destroy_body(chunk->body_id);
                chunk->body_id = b2_nullBodyId;
                chunk->chain_ids.clear();
            }
            chunk->debug_verts.clear();

            // Read back this chunk's pixels (exact chunk size, no padding)
            int chunk_w = std::min(m_chunk_size_x, grid.width() - chunk_px);
            int chunk_h = std::min(m_chunk_size_y, grid.height() - chunk_py);

            size_t pixel_size = grid.pixel_size();
            size_t buf_size = static_cast<size_t>(chunk_w) * chunk_h * pixel_size;
            m_terrain_readback_buf.resize(buf_size);
            grid.readback_region(chunk_px, chunk_py, chunk_w, chunk_h,
                                m_terrain_readback_buf.data(), static_cast<int>(buf_size));

            // Build solid grid from chunk
            // Can't use std::vector<bool> — it's bit-packed and has no .data()
            auto solid = std::make_unique<bool[]>(chunk_w * chunk_h);
            bool has_solid = false;
            bool has_moving = false;  // Track if chunk has moving pixels

            // Determine if this chunk is at the bottom of the grid
            bool at_grid_bottom = (chunk_py + chunk_h >= grid.height());

            for (int ly = 0; ly < chunk_h; ly++) {
                for (int lx = 0; lx < chunk_w; lx++) {
                    int i = ly * chunk_w + lx;
                    // Read category directly from byte 1 (engine-defined physics property)
                    uint8_t category = m_terrain_readback_buf[i * pixel_size + 1]; // byte 1

                    if (category == simulation::CAT_EMPTY) {
                        solid[i] = false;
                        continue;
                    }

                    // Static materials (rock, ice, etc.) always get colliders
                    if (category == simulation::CAT_STATIC) {
                        solid[i] = true;
                        has_solid = true;
                        continue;
                    }

                    // Powders only get colliders if they're "settled" (something below them)
                    if (category == simulation::CAT_POWDER) {
                        bool is_settled = false;

                        if (ly == chunk_h - 1) {
                            // Bottom row: settled if at grid bottom
                            is_settled = at_grid_bottom;
                            if (!is_settled) has_moving = true;
                        } else {
                            // Check the pixel directly below
                            int below_i = (ly + 1) * chunk_w + lx;
                            uint8_t below_cat = m_terrain_readback_buf[below_i * pixel_size + 1];
                            is_settled = (below_cat != simulation::CAT_EMPTY);
                            if (!is_settled) has_moving = true;
                        }

                        solid[i] = is_settled;
                        if (is_settled) has_solid = true;
                        continue;
                    }

                    // Liquids and gases never get terrain colliders (they flow freely)
                    solid[i] = false;
                }
            }

            // If this chunk has moving pixels, mark it and the chunk below dirty
            // for next frame so colliders update as materials settle
            if (has_moving) {
                chunk->dirty = true;
                // Mark chunk below as dirty too (where falling materials will land)
                if (cy + 1 < num_chunks_y) {
                    ChunkCoord below_coord{cx, cy + 1};
                    auto below_it = m_terrain_chunks.find(below_coord);
                    if (below_it != m_terrain_chunks.end()) {
                        below_it->second.dirty = true;
                    }
                }
            }

            if (!has_solid) continue;

            // Generate contour with minimal simplification
            auto contours = ContourGenerator::generate(solid.get(), chunk_w, chunk_h, 0.25f);

            for (auto& contour : contours) {
                if (contour.is_hole || contour.vertices.size() < 4) continue;

                // Convert contour vertices from grid-local Y-down to entity-local Y-up space.
                // Grid-local (gx, gy) Y-down → entity-local:
                //   entity_x = (gx - origin_x) * scale_x
                //   entity_y = (grid_height - gy - origin_y) * scale_y
                // Then convert to meters for Box2D body-local space.
                //
                // Reverse vertex order: marching squares produces CW winding
                // (negative signed area in Y-down), but Box2D chain shapes
                // require CCW winding so normals point outward.
                std::vector<b2Vec2> chain_verts(contour.vertices.size());
                std::vector<b2Vec2> debug_world(contour.vertices.size());

                for (size_t i = 0; i < contour.vertices.size(); i++) {
                    // Grid-local position (Y-down)
                    float gx = static_cast<float>(chunk_px) + contour.vertices[i].x;
                    float gy = static_cast<float>(chunk_py) + contour.vertices[i].y;

                    // Convert to entity-local Y-up, applying origin offset and scale
                    float local_x = (gx - static_cast<float>(transform.origin_x)) * transform.scale_x;
                    float local_y = (static_cast<float>(grid_h) - gy - static_cast<float>(transform.origin_y)) * transform.scale_y;

                    // Body-local meters (body is at entity world pos with entity rotation)
                    chain_verts[i] = m_world->pixels_to_meters(local_x, local_y);

                    // World-space pixels for debug rendering
                    debug_world[i].x = transform.world_x + local_x * e_cos - local_y * e_sin;
                    debug_world[i].y = transform.world_y + local_x * e_sin + local_y * e_cos;
                }
                std::reverse(chain_verts.begin(), chain_verts.end());
                std::reverse(debug_world.begin(), debug_world.end());

                // Create static body at entity's world position with entity's rotation
                if (!b2Body_IsValid(chunk->body_id)) {
                    chunk->body_id = m_world->create_static_body(
                        transform.world_x, transform.world_y, e_rot_rad);
                }

                // Add chain for this contour (multiple disconnected regions per chunk)
                b2ChainId chain = m_world->add_chain_shape(
                    chunk->body_id, chain_verts.data(),
                    static_cast<int>(chain_verts.size()));
                chunk->chain_ids.push_back(chain);
                chunk->debug_verts.push_back(std::move(debug_world));
            }
        }
    }
}

} // namespace engine::physics
