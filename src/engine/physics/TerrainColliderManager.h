#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include "box2d/box2d.h"

namespace engine::simulation { class PixelGrid; }

namespace engine::physics {

class PhysicsWorld;
class PixelBody;

struct ChunkCoord {
    int x, y;

    bool operator==(const ChunkCoord& other) const {
        return x == other.x && y == other.y;
    }
};

struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord& c) const noexcept {
        std::size_t seed = 0;

        std::hash<int> hasher;
        seed ^= hasher(c.x) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= hasher(c.y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);

        return seed;
    }
};

// Manages terrain collision generation in chunks
class TerrainColliderManager {
public:
    struct EntityTransform {
        float world_x = 0.0f;
        float world_y = 0.0f;
        float world_rotation_deg = 0.0f;
        float scale_x = 1.0f;
        float scale_y = 1.0f;
        int origin_x = 0;
        int origin_y = 0;
    };

    struct TerrainChunk {
        b2BodyId body_id = b2_nullBodyId;
        std::vector<b2ChainId> chain_ids;
        std::vector<std::vector<b2Vec2>> debug_verts;
        bool active = false;
        bool dirty = true;

        std::unique_ptr<bool[]> solid_grid;
        int solid_width = 0;
        int solid_height = 0;
        bool has_solid = false;
    };

    bool init(PhysicsWorld& world, int chunk_size_x = 32, int chunk_size_y = 32);
    void shutdown();

    void update_terrain_colliders(simulation::PixelGrid& grid,
                                   const EntityTransform& transform);

    void mark_dirty_region(int x, int y, int w, int h);
    void apply_gpu_dirty_flags(const std::vector<bool>& dirty_flags,
                                int num_chunks_x, int num_chunks_y);
    void mark_dirty_near_bodies(const std::vector<PixelBody*>& bodies,
                                 PhysicsWorld& world,
                                 float margin = 16.0f);

    const std::unordered_map<ChunkCoord, TerrainChunk, ChunkCoordHash>& terrain_chunks() const {
        return m_terrain_chunks;
    }

    int chunk_size_x() const { return m_chunk_size_x; }
    int chunk_size_y() const { return m_chunk_size_y; }

private:
    PhysicsWorld* m_world = nullptr;
    int m_chunk_size_x = 32;
    int m_chunk_size_y = 32;
    std::unordered_map<ChunkCoord, TerrainChunk, ChunkCoordHash> m_terrain_chunks;
    std::vector<uint8_t> m_terrain_readback_buf;
};

}
