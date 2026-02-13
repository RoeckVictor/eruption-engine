#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "box2d/box2d.h"

namespace engine::simulation { class PixelGrid; }

namespace engine::physics {

class PhysicsWorld;
class PixelBody;

/// Terrain chunk coordinate for hash map lookup
struct ChunkCoord {
    int x, y;

    bool operator==(const ChunkCoord& other) const {
        return x == other.x && y == other.y;
    }
};

/// Hash function for ChunkCoord using hash combining (safe for negative coordinates)
struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord& c) const noexcept {
        // Use hash combining pattern (similar to boost::hash_combine)
        // This safely handles signed integers without overflow
        std::size_t seed = 0;

        // Hash x coordinate
        std::hash<int> hasher;
        seed ^= hasher(c.x) + 0x9e3779b9 + (seed << 6) + (seed >> 2);

        // Hash y coordinate
        seed ^= hasher(c.y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);

        return seed;
    }
};

/// Manages terrain collision generation in chunks.
/// Generates Box2D chain colliders from the cellular automata grid
/// for static and settled materials (rock, settled sand, etc.).
class TerrainColliderManager {
public:
    /// Entity transform data for positioning terrain colliders in world space.
    struct EntityTransform {
        float world_x = 0.0f;
        float world_y = 0.0f;
        float world_rotation_deg = 0.0f;
        float scale_x = 1.0f;
        float scale_y = 1.0f;
        int origin_x = 0;      // Pixel grid origin/pivot X
        int origin_y = 0;      // Pixel grid origin/pivot Y
    };

    struct TerrainChunk {
        b2BodyId body_id = b2_nullBodyId;
        std::vector<b2ChainId> chain_ids;  // Multiple chains for disconnected regions
        std::vector<std::vector<b2Vec2>> debug_verts;  // World-space pixel coords per chain (for debug draw)
        bool active = false;
        bool dirty = true;
    };

    /// Initialize terrain collider manager.
    /// @param world Physics world to use
    /// @param chunk_size_x Width of terrain chunks in pixels
    /// @param chunk_size_y Height of terrain chunks in pixels
    bool init(PhysicsWorld& world, int chunk_size_x = 32, int chunk_size_y = 32);
    void shutdown();

    /// Update terrain colliders for the entire grid.
    /// Regenerates colliders for any chunks marked as dirty.
    /// @param grid Pixel grid to read terrain from
    /// @param transform Entity transform for positioning colliders in world space
    void update_terrain_colliders(simulation::PixelGrid& grid,
                                   const EntityTransform& transform);

    /// Mark terrain chunks overlapping a pixel-space rectangle as dirty.
    /// @param x X position in pixels
    /// @param y Y position in pixels
    /// @param w Width in pixels
    /// @param h Height in pixels
    void mark_dirty_region(int x, int y, int w, int h);

    /// Mark terrain chunks near dynamic bodies as dirty (accounts for CA changes).
    /// @param bodies List of bodies to check
    /// @param world Physics world for coordinate queries
    /// @param margin Additional margin in pixels around each body
    void mark_dirty_near_bodies(const std::vector<PixelBody*>& bodies,
                                 PhysicsWorld& world,
                                 float margin = 16.0f);

    /// Access terrain chunks (for debug visualization).
    const std::unordered_map<ChunkCoord, TerrainChunk, ChunkCoordHash>& terrain_chunks() const {
        return m_terrain_chunks;
    }

    /// Get chunk size in pixels.
    int chunk_size_x() const { return m_chunk_size_x; }
    int chunk_size_y() const { return m_chunk_size_y; }

private:
    PhysicsWorld* m_world = nullptr;
    int m_chunk_size_x = 32;
    int m_chunk_size_y = 32;
    std::unordered_map<ChunkCoord, TerrainChunk, ChunkCoordHash> m_terrain_chunks;
    std::vector<uint8_t> m_terrain_readback_buf;
};

} // namespace engine::physics
