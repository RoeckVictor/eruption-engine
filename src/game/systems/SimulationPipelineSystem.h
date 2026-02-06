#pragma once

#include "engine/core/System.h"
#include <entt/fwd.hpp>

namespace engine::physics { class PhysicsWorld; class PixelBodyManager; }
namespace engine::particles { class ParticleBuffer; class ParticleSimulation; }

namespace game {

class World;

/// Combined simulation pipeline per fixed timestep:
///   1. Step Box2D physics
///   2. Stamp rigid body pixels into grid
///   3. Run CA simulation (Margolus)
///   4. Clear stamps from grid
///   5. Reclaim dead particles & flush spawns
///   6. Run particle update compute shader
///   7. Run particle re-integration compute shader
///   8. Update terrain colliders
///   9. Recompute dirty shapes & handle splits
class SimulationPipelineSystem : public engine::System {
public:
    bool init(engine::Engine& engine) override;
    void fixed_update(engine::Engine& engine, float dt) override;

private:
    entt::registry* m_registry = nullptr;
    World* m_world = nullptr;
    engine::physics::PhysicsWorld* m_physics_world = nullptr;
    engine::physics::PixelBodyManager* m_body_manager = nullptr;
    engine::particles::ParticleBuffer* m_particle_buffer = nullptr;
    engine::particles::ParticleSimulation* m_particle_sim = nullptr;
};

} // namespace game
