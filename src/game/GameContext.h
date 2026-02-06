#pragma once

#include <entt/fwd.hpp>

namespace engine::render { struct Camera2D; }
namespace engine::physics { class PhysicsWorld; class PixelBodyManager; }
namespace engine::particles { class ParticleBuffer; class ParticleSimulation; class ParticleRenderer; }

namespace game {

class World;

struct GameContext {
    entt::registry& registry;
    World& world;
    engine::render::Camera2D& camera;
    engine::physics::PhysicsWorld& physics_world;
    engine::physics::PixelBodyManager& body_manager;
    engine::particles::ParticleBuffer& particle_buffer;
    engine::particles::ParticleSimulation& particle_sim;
    engine::particles::ParticleRenderer& particle_renderer;
};

} // namespace game
