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
    if (chunk_size_x <= 0 || chunk_size_y <= 0) {
        ENGINE_ERR("TerrainColliderManager::init() - Invalid chunk size (%d, %d)", chunk_size_x, chunk_size_y);
        return false;
    }

    m_world = &world;
    m_chunk_size_x = chunk_size_x;
    m_chunk_size_y = chunk_size_y;

    // Pre-allocate readback buffer for one chunk (avoids allocation on first dirty chunk).
    // Uses minimum pixel size of 4; will grow if pixel_size is larger.
    m_terrain_readback_buf.reserve(static_cast<size_t>(chunk_size_x) * chunk_size_y * 4);

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

void TerrainColliderManager::apply_gpu_dirty_flags(const std::vector<bool>& dirty_flags,
                                                     int num_chunks_x, int num_chunks_y) {
    // Apply dirty flags from GPU simulation
    // Only chunks where pixels actually moved are marked dirty
    for (int cy = 0; cy < num_chunks_y; cy++) {
        for (int cx = 0; cx < num_chunks_x; cx++) {
            int idx = cy * num_chunks_x + cx;
            if (idx >= static_cast<int>(dirty_flags.size())) continue;

            if (dirty_flags[idx]) {
                ChunkCoord coord{cx, cy};
                auto it = m_terrain_chunks.find(coord);
                if (it != m_terrain_chunks.end()) {
                    it->second.dirty = true;
                } else {
                    // Create chunk entry if it doesn't exist yet
                    m_terrain_chunks[coord].dirty = true;
                }
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

    // Track which chunks were updated this frame (for pass 2)
    std::vector<ChunkCoord> updated_chunks;
    updated_chunks.reserve(num_chunks_x * num_chunks_y / 4);  // rough estimate

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
            int chunk_w = std::min(m_chunk_size_x, std::max(0, grid.width() - chunk_px));
            int chunk_h = std::min(m_chunk_size_y, std::max(0, grid.height() - chunk_py));
            if (chunk_w <= 0 || chunk_h <= 0) continue;

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
                            // Only consider the powder settled if it rests on a solid
                            // surface (static material) or another powder that could
                            // itself be settled. Liquids and gases are moving materials
                            // and cannot support powder for collision purposes.
                            int below_i = (ly + 1) * chunk_w + lx;
                            uint8_t below_cat = m_terrain_readback_buf[below_i * pixel_size + 1];
                            is_settled = (below_cat == simulation::CAT_STATIC ||
                                          below_cat == simulation::CAT_POWDER);
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

            // Store solid grid in TerrainChunk for persistent neighbor lookups
            chunk->solid_grid = std::move(solid);
            chunk->solid_width = chunk_w;
            chunk->solid_height = chunk_h;
            chunk->has_solid = has_solid;

            // Track this chunk for pass 2
            updated_chunks.push_back(coord);
        }
    }

    for (const auto& coord : updated_chunks) {
        TerrainChunk* chunk = &m_terrain_chunks[coord];
        if (!chunk->has_solid) continue;

        int chunk_w = chunk->solid_width;
        int chunk_h = chunk->solid_height;
        int chunk_px = coord.x * m_chunk_size_x;
        int chunk_py = coord.y * m_chunk_size_y;

        // Build neighbor CORNER info only - edges would break closed contours
        // Corner info prevents diagonal dents at chunk boundary corners
        NeighborEdges neighbors;

        // Helper to get a chunk's solid grid from m_terrain_chunks
        auto get_chunk = [&](int cx, int cy) -> const TerrainChunk* {
            auto it = m_terrain_chunks.find({cx, cy});
            if (it != m_terrain_chunks.end() && it->second.solid_grid) {
                return &it->second;
            }
            return nullptr;
        };

        // Only set corner pixels from diagonal neighbors (no edge arrays)
        // This fixes corner dents without breaking contour closure
        if (auto* tl = get_chunk(coord.x - 1, coord.y - 1)) {
            neighbors.top_left = tl->solid_grid[((tl->solid_height - 1) * tl->solid_width) + (tl->solid_width - 1)];
        }
        if (auto* tr = get_chunk(coord.x + 1, coord.y - 1)) {
            neighbors.top_right = tr->solid_grid[(tr->solid_height - 1) * tr->solid_width];
        }
        if (auto* bl = get_chunk(coord.x - 1, coord.y + 1)) {
            neighbors.bottom_left = bl->solid_grid[bl->solid_width - 1];
        }
        if (auto* br = get_chunk(coord.x + 1, coord.y + 1)) {
            neighbors.bottom_right = br->solid_grid[0];
        }

        // Generate contours (corner info helps with diagonal neighbor awareness)
        auto contours = ContourGenerator::generate(chunk->solid_grid.get(), chunk_w, chunk_h, 0.25f, &neighbors);

        for (auto& contour : contours) {
            if (contour.is_hole || contour.vertices.size() < 4) continue;

            // Post-process: snap vertices near chunk boundaries to eliminate dents
            // Dents occur because marching squares creates small diagonal segments
            // at corners where chunk boundaries meet. Snapping straightens these.
            for (auto& v : contour.vertices) {
                if (v.x > 0.0f && v.x < 1.0f) v.x = 0.0f;
                if (v.x > static_cast<float>(chunk_w) - 1.0f && v.x < static_cast<float>(chunk_w)) {
                    v.x = static_cast<float>(chunk_w);
                }

                if (v.y > 0.0f && v.y < 1.0f) v.y = 0.0f;
                if (v.y > static_cast<float>(chunk_h) - 1.0f && v.y < static_cast<float>(chunk_h)) {
                    v.y = static_cast<float>(chunk_h);
                }
            }

            // Remove duplicate consecutive vertices (can occur after snapping)
            std::vector<Vec2f> deduped;
            deduped.reserve(contour.vertices.size());
            for (size_t i = 0; i < contour.vertices.size(); i++) {
                const auto& curr = contour.vertices[i];
                const auto& next = contour.vertices[(i + 1) % contour.vertices.size()];
                // Keep vertex if it's different from the next one
                if (std::abs(curr.x - next.x) > 0.01f || std::abs(curr.y - next.y) > 0.01f) {
                    deduped.push_back(curr);
                }
            }
            if (deduped.size() < 4) continue;  // Need at least 4 vertices for valid chain
            contour.vertices = std::move(deduped);

            // Convert contour vertices from grid-local Y-down to entity-local Y-up space.
            // Grid-local (gx, gy) Y-down → entity-local:
            //   entity_x = (gx - origin_x) * scale_x
            //   entity_y = (grid_height - gy - origin_y) * scale_y
            // Then convert to meters for Box2D body-local space.
            //
            // Winding note: Marching squares produces CCW in Y-down (negative signed area).
            // The Y-flip (Y-down → Y-up) reverses winding to CW, which gives inward-facing
            // normals for Box2D chain shapes. This is correct for terrain collision
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