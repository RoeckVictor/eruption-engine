#pragma once

#include "engine/physics/PhysicsWorld.h"
#include "engine/physics/PixelBody.h"
#include "engine/physics/PixelBodyStamper.h"
#include "engine/physics/TerrainColliderManager.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace engine::simulation { class PixelGrid; }
namespace engine::particles { class ParticleBuffer; }

namespace engine::physics {

/// Manages multiple PixelBody instances and coordinates the stamp/clear pipeline
/// for integrating rigid bodies with the cellular automata simulation.
///
/// The per-frame pipeline is:
///   1. step_physics()       — advance Box2D simulation
///   2. stamp_all()          — write rigid body pixels into the CA grid
///   3. [CA simulation runs]
///   4. clear_all()          — restore grid pixels that were overwritten
///   5. update_dirty_shapes()— recompute collision shapes for modified bodies
///   6. handle_splits()      — split disconnected bodies into separate entities
class PixelBodyManager {
public:
    /// Initialize the pixel body manager.
    /// @param world Physics world to use
    /// @param terrain_chunk_size Size of terrain collision chunks in pixels
    /// @param min_pixels Minimum pixel count for bodies to remain as rigid bodies
    bool init(PhysicsWorld& world, int terrain_chunk_size = 64, int min_pixels = 4);
    void shutdown();

    // --- Body lifecycle ---

    /// Create a new pixel body. Returns a non-owning pointer (manager owns the body).
    /// @param materials      Material IDs (game-defined, for rendering and game logic)
    /// @param categories     Physics categories (engine-defined, for collision behavior)
    /// @param indestructible If true, body pixels cannot be destroyed by tools/damage
    PixelBody* create_body(const uint8_t* materials, const uint8_t* categories, int w, int h,
                           float world_px, float world_py, bool is_dynamic = true,
                           bool indestructible = false);

    /// Destroy a pixel body and its Box2D body.
    void destroy_body(PixelBody* body);

    // --- Per-frame pipeline ---

    /// Step the Box2D physics world.
    void step_physics(float dt);

    /// Stamp all rigid body pixels into the CA grid.
    /// Reads current grid state, writes body pixels, records originals for clearing.
    /// If particle_buffer is provided, spawns particles when body pixels displace movable materials.
    void stamp_all(simulation::PixelGrid& grid, particles::ParticleBuffer* particle_buffer = nullptr);

    /// Restore grid pixels that were overwritten during stamping.
    void clear_all(simulation::PixelGrid& grid);

    /// Recompute collision shapes for bodies with dirty flags.
    void update_dirty_shapes();

    /// Check for and handle body splits (disconnected components).
    /// Returns the number of new bodies created from splits.
    int handle_splits();

    // --- Terrain collider management ---

    /// Update terrain colliders for the entire grid.
    /// Regenerates colliders for any chunks marked as dirty.
    void update_terrain_colliders(simulation::PixelGrid& grid,
                                  const TerrainColliderManager::EntityTransform& transform);

    /// Mark terrain chunks overlapping a pixel-space rectangle as dirty.
    void mark_terrain_dirty_region(int x, int y, int w, int h);

    /// Mark terrain chunks near each dynamic body as dirty (accounts for CA changes).
    void mark_terrain_dirty_near_bodies(float margin = 16.0f);

    // --- Access ---

    const std::vector<std::unique_ptr<PixelBody>>& bodies() const { return m_bodies; }
    PhysicsWorld* physics_world() { return m_world; }

    /// Access terrain collider manager (for debug visualization).
    const TerrainColliderManager& terrain_colliders() const { return m_terrain_colliders; }

    /// Minimum pixel count for a body to remain as a rigid body.
    /// Bodies smaller than this are destroyed (caller can convert to particles).
    int min_body_pixels = 4;

private:
    PhysicsWorld* m_world = nullptr;
    std::vector<std::unique_ptr<PixelBody>> m_bodies;
    std::vector<PixelBody*> m_pending_destroy;

    // Delegated components
    PixelBodyStamper m_stamper;
    TerrainColliderManager m_terrain_colliders;
};

} // namespace engine::physics
