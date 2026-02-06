#include "game/systems/SimulationPipelineSystem.h"
#include "engine/core/Engine.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/physics/PixelBodyManager.h"
#include "engine/particles/ParticleBuffer.h"
#include "engine/particles/ParticleSimulation.h"
#include "engine/render/Camera2D.h"
#include "game/GameContext.h"
#include "game/components/Components.h"
#include "game/world/World.h"
#include <entt/entt.hpp>

namespace game {

bool SimulationPipelineSystem::init(engine::Engine& engine) {
    auto& ctx = engine.app_context<GameContext>();
    m_registry = &ctx.registry;
    m_world = &ctx.world;
    m_physics_world = &ctx.physics_world;
    m_body_manager = &ctx.body_manager;
    m_particle_buffer = &ctx.particle_buffer;
    m_particle_sim = &ctx.particle_sim;
    return true;
}

void SimulationPipelineSystem::fixed_update(engine::Engine& engine, float dt) {
    const auto& state = m_registry->ctx().get<const GameInputState>();
    if (state.sim_paused) return;

    auto& grid = m_world->grid();
    auto& ctx_ref = engine.render_context();

    // 1. Clear stamps from previous frame (they persisted through render)
    m_body_manager->clear_all(grid);

    // 2. Update terrain colliders BEFORE stamping (reads clean grid without body pixels)
    m_body_manager->update_terrain_colliders(grid);

    // 3. Step Box2D physics
    m_body_manager->step_physics(dt);

    // 4. Run CA simulation (Margolus phases) on clean grid (no body pixels).
    //    Body pixels must NOT be stamped during CA — the CA would treat them
    //    as regular materials and move them, leaking pixels into the world.
    m_world->simulate(ctx_ref);

    // 4b. Mark terrain colliders dirty near dynamic bodies (CA may have changed terrain)
    m_body_manager->mark_terrain_dirty_near_bodies();

    // 5. Stamp rigid body pixels into grid (for particle collision + render).
    //    Stamps persist until next frame's clear_all.
    //    Pass particle buffer to spawn particles when displacing movable materials.
    m_body_manager->stamp_all(grid, m_particle_buffer);

    // 6. Reclaim dead particles from previous frame & flush spawn queue
    m_particle_buffer->reclaim_dead();
    m_particle_buffer->flush_spawns();
    m_particle_buffer->reset_dead_counter();

    // 7. Run particle update compute shader (physics + grid collision)
    m_particle_sim->update(*m_particle_buffer, grid, ctx_ref, dt);

    // 8. Run particle re-integration (settled particles → grid pixels)
    m_particle_sim->reintegrate(*m_particle_buffer, grid, ctx_ref);

    // 9. Handle body splits first (uses dirty flag + pixel buffer, not shapes),
    //    then recompute collision shapes for remaining dirty bodies.
    m_body_manager->handle_splits();
    m_body_manager->update_dirty_shapes();

    // 10. Update render texture from SSBO (copy material IDs for palette-based rendering)
    grid.update_render_texture();
}

} // namespace game
